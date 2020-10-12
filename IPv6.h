#pragma once

#include <iostream>
#include <cstdint>

static inline void print_ipv6(std::ostream & out, uint16_t ip[8], unsigned short port) {
	out << std::hex << "[";
	int compact0 = -1;
	for (int i = 0; i < 8; i++) {
		unsigned x = ip[i];

		if (x == 0) {
			if (compact0 == -1 && i < 7) {
				compact0 = 1;
				continue;
			}
			else if (compact0 != -999 && i < 7) {
				compact0++;
				continue;
			}
		}
		else {
			if (compact0 == 1) {
				out << "0:";
				compact0 = -999;
			}
			else if (compact0 > 1) {
				if (compact0 == 7) {
					out << ":";

				}
				out << ":";
				compact0 = -999;
			}
		}
		out << x;

		if (i != 7) {
			out << ":";
		}
	}
	out << "]:" << std::dec << port;
}
