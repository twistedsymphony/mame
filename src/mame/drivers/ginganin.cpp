// license:BSD-3-Clause
// copyright-holders:Luca Elia
/***************************************************************************

                            Ginga NinkyouDen
                            (C) 1987 Jaleco

                    driver by Luca Elia (l.elia@tin.it)

CPU   : 68000 68B09
SOUND : YM2149 Y8950(MSX AUDIO)
OSC.  : 6.000MHz 3.579545MHz

* CTC uses MB-8873E (MC-6840)

                    Interesting routines (main cpu)
                    -------------------------------

Interrupts: 1-7]    d17a:   clears 20018 etc.

f4b2    print string:   a1->(char)*,0x25(%) d7.w=color  a0->screen (30000)
f5d6    print 7 digit BCD number: d0.l to (a1)+ color $3000


                    Interesting locations (main cpu)
                    --------------------------------

20014   # of players (1-2)
20018   cleared by interrupts
2001c   credits (max 9)
20020   internal timer?
20024   initial lives
20058   current lives p1
2005c   current lives p2
20070   coins
200a4   time
200a8   energy

60008       values: 0 1 ffff
6000c       bit:    0   flip sceen? <-  70002>>14
                    1   ?           <-

6000e   soundlatch  <- 20038 2003c 20040


                                To Do
                                -----

- game doesn't init paletteram / tilemaps properly, ending up with MAME
  palette defaults at start-up and missing text layer if you coin it up
  too soon.

***************************************************************************/

#include "emu.h"
#include "includes/ginganin.h"

#include "cpu/m68000/m68000.h"
#include "cpu/m6809/m6809.h"
#include "machine/6840ptm.h"
#include "sound/ay8910.h"
#include "sound/8950intf.h"

#include "screen.h"
#include "speaker.h"


#define MAIN_CLOCK XTAL(6'000'000)
#define SOUND_CLOCK XTAL(3'579'545)


/*
**
**              Main cpu data
**
*/


void ginganin_state::ginganin_map(address_map &map)
{
/* The ROM area: 10000-13fff is written with: 0000 0000 0000 0001, at startup only. Why? */
	map(0x000000, 0x01ffff).rom();
	map(0x020000, 0x023fff).ram();
	map(0x030000, 0x0307ff).ram().w(FUNC(ginganin_state::ginganin_txtram16_w)).share("txtram");
	map(0x040000, 0x0407ff).ram().share("spriteram");
	map(0x050000, 0x0507ff).ram().w(m_palette, FUNC(palette_device::write16)).share("palette");
	map(0x060000, 0x06000f).ram().w(FUNC(ginganin_state::ginganin_vregs16_w)).share("vregs");
	map(0x068000, 0x06bfff).ram().w(FUNC(ginganin_state::ginganin_fgram16_w)).share("fgram");
	map(0x070000, 0x070001).portr("P1_P2");
	map(0x070002, 0x070003).portr("DSW");
}


/*
**
**              Sound cpu data
**
*/

void ginganin_state::sound_map(address_map &map)
{
	map(0x0000, 0x07ff).ram();
	map(0x0800, 0x0807).rw("6840ptm", FUNC(ptm6840_device::read), FUNC(ptm6840_device::write));
	map(0x1800, 0x1800).r(m_soundlatch, FUNC(generic_latch_8_device::read));
	map(0x2000, 0x2001).w("ymsnd", FUNC(y8950_device::write));
	map(0x2800, 0x2801).w("psg", FUNC(ym2149_device::address_data_w));
	map(0x4000, 0xffff).rom();
}


/* Input Ports */

static INPUT_PORTS_START( ginganin )
	PORT_START("P1_P2")     /* 70000.w */
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_START2 )

	PORT_START("DSW")       /* 70002.w */
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x0038, 0x0038, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0038, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0018, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0028, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x0040, 0x0040, "Infinite Lives")
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, "Free Play & Invulnerability")
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_DIPNAME( 0x0300, 0x0300, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0000, "2")
	PORT_DIPSETTING(      0x0300, "3")
	PORT_DIPSETTING(      0x0100, "4")
	PORT_DIPSETTING(      0x0200, "5")
	PORT_DIPNAME( 0x0400, 0x0400, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0400, DEF_STR( On ) )
	PORT_DIPNAME( 0x0800, 0x0000, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Upright ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( Cocktail ) )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Unknown ) )  /* probably unused */
	PORT_DIPSETTING(      0x1000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Unknown ) )  /* it does something */
	PORT_DIPSETTING(      0x2000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x8000, 0x8000, "Freeze" )
	PORT_DIPSETTING(      0x8000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
INPUT_PORTS_END



/*
**
**              Gfx data
**
*/


#define layout16x16(_name_,_romsize_) \
static const gfx_layout _name_ =\
{\
	16,16,\
	(_romsize_)*8/(16*16*4),\
	4,\
	{0, 1, 2, 3},\
	{0*4,1*4,2*4,3*4,4*4,5*4,6*4,7*4,\
		0*4+32*16,1*4+32*16,2*4+32*16,3*4+32*16,4*4+32*16,5*4+32*16,6*4+32*16,7*4+32*16},\
	{0*32,1*32,2*32,3*32,4*32,5*32,6*32,7*32,\
		8*32,9*32,10*32,11*32,12*32,13*32,14*32,15*32},\
	16*16*4\
};

#define layout8x8(_name_,_romsize_) \
static const gfx_layout _name_ =\
{\
	8,8,\
	(_romsize_)*8/(8*8*4),\
	4,\
	{0, 1, 2, 3},\
	{0*4,1*4,2*4,3*4,4*4,5*4,6*4,7*4}, \
	{0*32,1*32,2*32,3*32,4*32,5*32,6*32,7*32},\
	8*8*4\
};

layout16x16(tilelayout,  0x20000)
layout8x8  (txtlayout,   0x04000)
layout16x16(spritelayout,0x50000)

static GFXDECODE_START( gfx_ginganin )
	GFXDECODE_ENTRY( "gfx1", 0, tilelayout,  256*3, 16 ) /* [0] bg */
	GFXDECODE_ENTRY( "gfx2", 0, tilelayout,  256*2, 16 ) /* [1] fg */
	GFXDECODE_ENTRY( "gfx3", 0, txtlayout,   256*0, 16 ) /* [2] txt */
	GFXDECODE_ENTRY( "gfx4", 0, spritelayout, 256*1, 16 ) /* [3] sprites */
GFXDECODE_END


void ginganin_state::machine_start()
{
	save_item(NAME(m_layers_ctrl));
	save_item(NAME(m_flipscreen));
}

void ginganin_state::machine_reset()
{
	m_layers_ctrl = 0;
	m_flipscreen = 0;
}

WRITE_LINE_MEMBER(ginganin_state::ptm_irq)
{
	m_audiocpu->set_input_line(0, state ? ASSERT_LINE : CLEAR_LINE);
}


MACHINE_CONFIG_START(ginganin_state::ginganin)

	/* basic machine hardware */
	MCFG_DEVICE_ADD("maincpu", M68000, MAIN_CLOCK)
	MCFG_DEVICE_PROGRAM_MAP(ginganin_map)
	MCFG_DEVICE_VBLANK_INT_DRIVER("screen", ginganin_state,  irq1_line_hold) /* ? (vectors 1-7 cointain the same address) */

	MCFG_DEVICE_ADD("audiocpu", MC6809, SOUND_CLOCK) // MBL68B09?
	MCFG_DEVICE_PROGRAM_MAP(sound_map)


	ptm6840_device &ptm(PTM6840(config, "6840ptm", SOUND_CLOCK/2));
	ptm.set_external_clocks(0, 0, 0);
	ptm.o1_callback().set(FUNC(ginganin_state::ptm_irq));

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MCFG_SCREEN_SIZE(256, 256)
	MCFG_SCREEN_VISIBLE_AREA(0, 255, 0 + 16 , 255 - 16)
	MCFG_SCREEN_UPDATE_DRIVER(ginganin_state, screen_update_ginganin)
	MCFG_SCREEN_PALETTE("palette")

	MCFG_DEVICE_ADD("gfxdecode", GFXDECODE, "palette", gfx_ginganin)
	MCFG_PALETTE_ADD("palette", 1024)
	MCFG_PALETTE_FORMAT(RRRRGGGGBBBBxxxx)

	/* sound hardware */
	SPEAKER(config, "mono").front_center();

	MCFG_GENERIC_LATCH_8_ADD("soundlatch")

	MCFG_DEVICE_ADD("psg", YM2149, SOUND_CLOCK / 2)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.10)

	MCFG_DEVICE_ADD("ymsnd", Y8950, SOUND_CLOCK) /* The Y8950 is basically a YM3526 with ADPCM built in */
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.0)
MACHINE_CONFIG_END



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( ginganin )
	ROM_REGION( 0x20000, "maincpu", 0 ) /* main cpu */
	ROM_LOAD16_BYTE( "gn_02.bin", 0x00000, 0x10000, CRC(4a4e012f) SHA1(7c94a5b6b71e037af355f3aa4623be1f585db8dc) )
	ROM_LOAD16_BYTE( "gn_01.bin", 0x00001, 0x10000, CRC(30256fcb) SHA1(dc15e0da88ae5cabe0150f7290508c3d58c06c11) )

	ROM_REGION( 0x10000, "audiocpu", 0 )    /* sound cpu */
	ROM_LOAD( "gn_05.bin", 0x00000, 0x10000, CRC(e76e10e7) SHA1(b16f10a1a01b7b04221c9bf1b0d157e936bc5fb5) )

	ROM_REGION( 0x20000, "gfx1", 0 )
	ROM_LOAD( "gn_15.bin", 0x000000, 0x10000, CRC(1b8ac9fb) SHA1(1e5ee2a565fa262f1e48c1088d84c6f42d84b4e3) )  /* bg */
	ROM_LOAD( "gn_14.bin", 0x010000, 0x10000, CRC(e73fe668) SHA1(fa39fddd7448d3fc6b539506e33b951db205afa1) )

	ROM_REGION( 0x20000, "gfx2", 0 )
	ROM_LOAD( "gn_12.bin", 0x000000, 0x10000, CRC(c134a1e9) SHA1(8bace0f0169e61f1b7254393fa9cad6dca09c335) )  /* fg */
	ROM_LOAD( "gn_13.bin", 0x010000, 0x10000, CRC(1d3bec21) SHA1(305823c78cad9288f918178e1c24cb0459ba2a6e) )

	ROM_REGION( 0x04000, "gfx3", 0 )
	ROM_LOAD( "gn_10.bin", 0x000000, 0x04000, CRC(ae371b2d) SHA1(d5e03b085586ed2bf40713f432bcf12e07318226) )  /* txt */

	ROM_REGION( 0x50000, "gfx4", 0 )
	ROM_LOAD( "gn_06.bin", 0x000000, 0x10000, CRC(bdc65835) SHA1(53222fc3ec15e641289abb754657b0d59b88b66b) )  /* sprites */
	ROM_CONTINUE(          0x040000, 0x10000 )
	ROM_LOAD( "gn_07.bin", 0x010000, 0x10000, CRC(c2b8eafe) SHA1(a042a200efd4e7361e9ab516085c9fc8067e28b4) )
	ROM_LOAD( "gn_08.bin", 0x020000, 0x10000, CRC(f7c73c18) SHA1(102700e2217bcd1532af56ee6a00ad608c8217db) )
	ROM_LOAD( "gn_09.bin", 0x030000, 0x10000, CRC(a5e07c3b) SHA1(cdda02cd847330575612cb33d1bb38a5d50a3e6d) )

	ROM_REGION( 0x08000, "gfx5", 0 )    /* background tilemaps */
	ROM_LOAD( "gn_11.bin", 0x00000, 0x08000, CRC(f0d0e605) SHA1(0c541e8e036573be1d99ecb71fdb4568ca8cc269) )

	ROM_REGION( 0x20000, "ymsnd", 0 )   /* samples */
	ROM_LOAD( "gn_04.bin", 0x00000, 0x10000, CRC(0ed9133b) SHA1(77f628e8ec28016efac2d906146865ca4ec54bd5) )
	ROM_LOAD( "gn_03.bin", 0x10000, 0x10000, CRC(f1ba222c) SHA1(780c0bd0045bac1e1bb3209576383db90504fbf3) )

ROM_END

ROM_START( ginganina )
	ROM_REGION( 0x20000, "maincpu", 0 ) /* main cpu */
	ROM_LOAD16_BYTE( "2.bin", 0x00000, 0x10000, CRC(6da1d8a3) SHA1(ea81f2934fa7901563e886f3d600edd08ec0ea24) )
	ROM_LOAD16_BYTE( "1.bin", 0x00001, 0x10000, CRC(0bd32d59) SHA1(5ab2c0e4a1d9cafbd3448d981103508debd7ed96) )

	ROM_REGION( 0x10000, "audiocpu", 0 )    /* sound cpu */
	ROM_LOAD( "gn_05.bin", 0x00000, 0x10000, CRC(e76e10e7) SHA1(b16f10a1a01b7b04221c9bf1b0d157e936bc5fb5) )

	ROM_REGION( 0x20000, "gfx1", 0 )
	ROM_LOAD( "gn_15.bin", 0x000000, 0x10000, CRC(1b8ac9fb) SHA1(1e5ee2a565fa262f1e48c1088d84c6f42d84b4e3) )  /* bg */
	ROM_LOAD( "gn_14.bin", 0x010000, 0x10000, CRC(e73fe668) SHA1(fa39fddd7448d3fc6b539506e33b951db205afa1) )

	ROM_REGION( 0x20000, "gfx2", 0 )
	ROM_LOAD( "gn_12.bin", 0x000000, 0x10000, CRC(c134a1e9) SHA1(8bace0f0169e61f1b7254393fa9cad6dca09c335) )  /* fg */
	ROM_LOAD( "gn_13.bin", 0x010000, 0x10000, CRC(1d3bec21) SHA1(305823c78cad9288f918178e1c24cb0459ba2a6e) )

	ROM_REGION( 0x04000, "gfx3", 0 )
	ROM_LOAD( "10.bin", 0x000000, 0x04000, CRC(48a20745) SHA1(69855b0402feca4ba9632142e569c652ca05b9fa) )  /* txt */

	ROM_REGION( 0x50000, "gfx4", 0 )
	ROM_LOAD( "gn_06.bin", 0x000000, 0x10000, CRC(bdc65835) SHA1(53222fc3ec15e641289abb754657b0d59b88b66b) )  /* sprites */
	ROM_CONTINUE(          0x040000, 0x10000 )
	ROM_LOAD( "gn_07.bin", 0x010000, 0x10000, CRC(c2b8eafe) SHA1(a042a200efd4e7361e9ab516085c9fc8067e28b4) )
	ROM_LOAD( "gn_08.bin", 0x020000, 0x10000, CRC(f7c73c18) SHA1(102700e2217bcd1532af56ee6a00ad608c8217db) )
	ROM_LOAD( "gn_09.bin", 0x030000, 0x10000, CRC(a5e07c3b) SHA1(cdda02cd847330575612cb33d1bb38a5d50a3e6d) )

	ROM_REGION( 0x08000, "gfx5", 0 )    /* background tilemaps */
	ROM_LOAD( "gn_11.bin", 0x00000, 0x08000, CRC(f0d0e605) SHA1(0c541e8e036573be1d99ecb71fdb4568ca8cc269) )

	ROM_REGION( 0x20000, "ymsnd", 0 )   /* samples */
	ROM_LOAD( "gn_04.bin", 0x00000, 0x10000, CRC(0ed9133b) SHA1(77f628e8ec28016efac2d906146865ca4ec54bd5) )
	ROM_LOAD( "gn_03.bin", 0x10000, 0x10000, CRC(f1ba222c) SHA1(780c0bd0045bac1e1bb3209576383db90504fbf3) )
ROM_END



void ginganin_state::init_ginganin()
{
	/* main cpu patches */
	uint16_t *rom = (uint16_t *)memregion("maincpu")->base();
	/* avoid writes to rom getting to the log */
	rom[0x408 / 2] = 0x6000;
	rom[0x40a / 2] = 0x001c;


	/* sound cpu patches */
	/* let's clear the RAM: ROM starts at 0x4000 */
	memset(memregion("audiocpu")->base(), 0, 0x800);
}


GAME( 1987, ginganin,  0,        ginganin, ginganin, ginganin_state, init_ginganin, ROT0, "Jaleco", "Ginga NinkyouDen (set 1)", MACHINE_SUPPORTS_SAVE )
GAME( 1987, ginganina, ginganin, ginganin, ginganin, ginganin_state, init_ginganin, ROT0, "Jaleco", "Ginga NinkyouDen (set 2)", MACHINE_SUPPORTS_SAVE )
