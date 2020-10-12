#pragma once

#include <cstdint>
#include <optional>
#include <functional>

namespace Network {
	struct IP {
		bool isV6;
		union {
			uint32_t v4;
			uint8_t v4_array[4];
			uint8_t v6[16];
			uint16_t v6_u16[8];
		};

		IP() {
			memset(v6, 16, 0);
		}

		IP(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
			isV6 = false;
			v4_array[0] = a;
			v4_array[1] = b;
			v4_array[2] = c;
			v4_array[3] = d;
		}
	};	

	

	// Called by main thread
	void global_init();

	void global_deinit();
};

