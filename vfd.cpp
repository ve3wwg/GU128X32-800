//////////////////////////////////////////////////////////////////////
// vfd.cpp -- VFD Driver Program
// Date: Sun Sep  2 22:13:23 2018   (C) ve3wwg@gmail.com
///////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>

#include <string>

#include "gp.h"
#include "ugui.h"

class Vfd {
	std::string	device;
	uint8_t		spimode = SPI_MODE_2;	// Clocked on rising edge
	uint32_t	speed = 200000u;	// 200 kHz
	int		gpio_rxd = 10;
	int		gpio_cd = 5;
	int		gpio_sck = 11;		// SPI_SCK
	int		gpio_css = 20;		// /CSS
	int		gpio_frp = 6;		// FRP (input)
	int		gpio_reset = 16;	// /RESET
	int		fd = -1;		// Open device

	UG_GUI		gui;
	uint8_t		pixmap[128*64/8];
	
public:
	enum class CmdData {
		Data=0,
		Cmd=1
	};
protected:
	inline void chip_select(bool cs) noexcept	{ gpio_write(gpio_css,!cs); }
	inline void cmd_data(CmdData which) noexcept	{ gpio_write(gpio_cd,int(which)); }
	
	int cs_write(const uint8_t *buf,int bytes) noexcept;
	int cs_write(uint8_t byte) noexcept;
	void gwrite(short x,short y,uint8_t vrow) noexcept;
	bool wcmd(uint8_t byte) noexcept;
	bool wdata(uint8_t byte) noexcept;

	static int ug_to_pen(UG_COLOR c) noexcept;
	static UG_COLOR pen_to_ug(int pen) noexcept;
	static void local_draw_point(UG_S16 x,UG_S16 y,UG_COLOR c,void *arg);

	uint8_t *to_pixel(short x,short y,unsigned *bitno) noexcept;

public:	Vfd() : device("/dev/spidev0.1") {}
	~Vfd() { close(); }

	void init();
	void close();

	void display_clear();
	void update() noexcept;

	Vfd& set_foreground(short pen) noexcept;
	Vfd& set_background(short pen) noexcept;
	Vfd& putstring(short x,short y,const char *buf) noexcept;
	Vfd& draw_point(short x,short y,short pen) noexcept;
	Vfd& draw_line(short x1,short y1,short x2,short y2,short pen) noexcept;
	Vfd& draw_frame(short x1,short y1,short x2,short y2,short pen) noexcept;
	Vfd& round_frame(short x1,short y1,short x2,short y2,short r,short pen) noexcept;

	UG_GUI *get_gui() { return &gui; }
} vfd;

void
Vfd::local_draw_point(UG_S16 x,UG_S16 y,UG_COLOR c,void *arg) {
	Vfd *v = (Vfd*)arg;

	v->draw_point(x,y,ug_to_pen(c));
}

void
Vfd::init() {
	int bits = 8;
	int rc;

	memset(pixmap,0,128*64/8);
	UG_Init(&gui,Vfd::local_draw_point,this,128,32);
	UG_SetBackcolor(&gui,pen_to_ug(0));
	UG_SetForecolor(&gui,pen_to_ug(1));
	UG_FontSelect(&gui,&FONT_8X14);
	UG_FontSetHSpace(&gui,0);

	gpio_write(gpio_rxd,1);
	gpio_write(gpio_cd,1);
	gpio_write(gpio_css,1);
	gpio_write(gpio_sck,1);
	gpio_write(gpio_reset,1);

	gpio_configure_io(gpio_rxd,Output);
	gpio_configure_io(gpio_cd,Output);
	gpio_configure_io(gpio_sck,Output);
	gpio_configure_io(gpio_css,Output);
	gpio_configure_io(gpio_reset,Output);

	gpio_configure_io(gpio_rxd,Alt0);	// MOSI
	gpio_configure_io(gpio_sck,Alt0);	// SCK

	gpio_configure_io(gpio_frp,Input);
	gpio_configure_pullup(gpio_frp,Up);

	fd = ::open(device.c_str(),O_RDWR);
	if ( fd < 0 ) {
		fprintf(stderr,"%s: opening %s for R/W\n",
			strerror(errno),
			device.c_str());
		exit(2);
	}

	assert(fd>=0);
	rc = ::ioctl(fd,SPI_IOC_WR_MODE,&spimode);
	assert(!rc);
	rc = ::ioctl(fd,SPI_IOC_RD_BITS_PER_WORD,&bits);
	assert(!rc);
	rc = ::ioctl(fd,SPI_IOC_WR_MAX_SPEED_HZ,&speed);
	assert(!rc);

	gpio_write(gpio_reset,1);
	gpio_write(gpio_reset,0);
	usleep(20);
	gpio_write(gpio_reset,1);
	usleep(1100);

	display_clear();
	assert(fd>=0);
}

void
Vfd::display_clear() {

	wcmd(0x5F);	// Display clear
	usleep(1100);	// Wait at least 1 ms

	// Display area set
	for ( int x=0; x<8; ++x ) {
		wcmd(0x62);
		wcmd(x & 0x0F);
		wdata(0xFF);
	}

	wcmd(0x24);	// Display L0 on
	wcmd(0x40);	// GS & !GRV

	wcmd(0x80);	// Fixed XY address

	wcmd(0x70);	// GRAM position start
	wcmd(0x00);	// X
	wcmd(0x00);	// Y

#if 0
	for ( int y=0; y < 4; ++y )
		for ( int x=0; x < 128; ++x )
			gwrite(x,y,x);
#endif
}

int
Vfd::cs_write(const uint8_t *buf,int bytes) noexcept {
	spi_ioc_transfer xfer;
	int rc;

	assert(fd>=0);
	chip_select(true);

	memset(&xfer,0,sizeof xfer);
	xfer.tx_buf = (unsigned long) buf;
	xfer.len = bytes;
	xfer.rx_buf = 0;
	xfer.cs_change = 0;
	rc = ::ioctl(fd,SPI_IOC_MESSAGE(1),&xfer);
	assert(rc == bytes);
	chip_select(false);
	return rc;
}

int
Vfd::cs_write(uint8_t byte) noexcept {
	int rc = cs_write(&byte,1);
	return rc;
}

bool
Vfd::wcmd(uint8_t byte) noexcept {
	cmd_data(CmdData::Cmd);
	return cs_write(byte) == 1;
}

bool
Vfd::wdata(uint8_t byte) noexcept {
	cmd_data(CmdData::Data);
	return cs_write(byte) == 1;
}

void
Vfd::gwrite(short x,short y,uint8_t vrow) noexcept {

	// Y:
	wcmd(0x60);
	wcmd(uint8_t(y));
	// X:
	wcmd(0x64);
	wcmd(uint8_t(x));
	wdata(vrow);
}

void
Vfd::update() noexcept {
	uint8_t *pp = pixmap;

	for ( short y=0; y<4; ++y )
		for ( short x=0; x<128; ++x )
			gwrite(x,y,*pp++);
}

void
Vfd::close() {
	if ( fd >= 0 ) {
		::close(fd);
		fd = -1;
	}
}

uint8_t *
Vfd::to_pixel(short x,short y,unsigned *bitno) noexcept {
	static uint8_t dummy;

	*bitno = 7 - y % 8;	// Inverted

	if ( x < 0 || x >= 128
	  || y < 0 || y >= 64 ) {
		assert(0);
	  	return &dummy;
	}

	unsigned pageno = y / 8;
	unsigned colno = x % 128;
	return &this->pixmap[pageno * 128 + colno];
}

Vfd&
Vfd::draw_point(short x,short y,short pen) noexcept {
	unsigned bitno;

	if ( x < 0 || x >= 128 || y < 0 || y >= 64 )
		return *this;

	uint8_t *byte = to_pixel(x,y,&bitno);
	uint8_t mask = 0x80 >> bitno;
	
	switch ( pen ) {
	case 0:
		*byte &= ~mask;
		break;
	case 1:
		*byte |= mask;
		break;
	default:
		*byte ^= mask;
	}
	return *this;
}

int
Vfd::ug_to_pen(UG_COLOR c) noexcept {

	switch ( c ) {
	case C_BLACK:
		return 0;
	case C_RED:
		return 2;
	default:
		return 1;
	}
}

UG_COLOR
Vfd::pen_to_ug(int pen) noexcept {

	switch ( pen ) {
	case 0:
		return C_BLACK;
	case 2:
		return C_RED;
	default:
		return C_WHITE;
	}	
}

Vfd&
Vfd::set_foreground(short pen) noexcept {
	UG_SetForecolor(&gui,pen_to_ug(pen));
	return *this;
}

Vfd&
Vfd::set_background(short pen) noexcept {
	UG_SetBackcolor(&gui,pen_to_ug(pen));
	return *this;
}

Vfd&
Vfd::putstring(short x,short y,const char *buf) noexcept {
	UG_PutString(&gui,x,y,buf);
	return *this;
}

Vfd&
Vfd::draw_line(short x1,short y1,short x2,short y2,short pen) noexcept {
	UG_DrawLine(&gui,x1,y1,x2,y2,pen_to_ug(pen));
	return *this;
}

Vfd&
Vfd::draw_frame(short x1,short y1,short x2,short y2,short pen) noexcept {
	UG_DrawFrame(&gui,x1,y1,x2,y2,pen_to_ug(pen));
	return *this;
}

Vfd&
Vfd::round_frame(short x1,short y1,short x2,short y2,short r,short pen) noexcept {
	UG_DrawRoundFrame(&gui,x1,y1,x2,y2,r,pen_to_ug(pen));
	return *this;
}

int
main(int argc,char **argv) {
	
	if ( !gpio_open() ) {
		fprintf(stderr,"Unable to open gpio\n");
		exit(1);
	}

	vfd.init();
	vfd.set_foreground(1);
	vfd.set_background(0);
	vfd.putstring(3,10,"-0123456789E+00");
	vfd.round_frame(0,6,126,25,5,1);
	vfd.draw_line(0,0,127,0,1);
	vfd.draw_line(0,31,127,31,1);
	vfd.draw_line(0,0,127,31,2);
	vfd.draw_line(0,31,127,0,2);
	vfd.update();
	gpio_close();
}

// End vfd.cpp
