#pragma once

#include <ruby.h>

extern "C" {
	extern VALUE Protocol_HTTP3;

	void Init_Protocol_HTTP3(void);
}
