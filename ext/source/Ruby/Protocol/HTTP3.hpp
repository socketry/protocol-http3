#pragma once

#include <ruby.h>

extern "C" {
	extern VALUE Ruby_Protocol_HTTP3;

	void Init_Ruby_Protocol_HTTP3(void);
}
