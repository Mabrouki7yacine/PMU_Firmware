#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#define HEADER_SIZE (1 + 4 + 8)

namespace Packet {
    enum class Type : uint8_t  { PMU = 1, Battery = 2, PV = 3, AC3 = 4, Wind = 5 };

	namespace Structure {
		struct PMU_struct_t {
			float v_a_mag, v_a_ang, v_b_mag, v_b_ang, v_c_mag, v_c_ang;
			float i_a_mag, i_a_ang, i_b_mag, i_b_ang, i_c_mag, i_c_ang;
		};
		struct Battery_struct_t { 
			float battery_v, battery_i, battery_temp;
		};
		struct PV_struct_t { 
			float pv_v, pv_i;
		};
		struct Wind_struct_t {
			float wind_v, wind_i, wind_power, wind_speed, rotor_rpm, ambient_temp;
		};
		struct AC_struct_t {
			float v_rms[3], i_rms[3], p_active[3], q_reactive[3], pf[3], frequency[3];
		};

    }

	template <Type T, typename Payload>
	class DataPacket
	{
	// Ai suggested this
    static_assert(std::is_trivially_copyable_v<Payload>, "payload must be plain data");
    static_assert(sizeof(Payload) % sizeof(float) == 0, "payload must be all floats");
	private:
		uint32_t device_ID;
		double   timestamp;
		Payload  payload_;
	public:
    	static constexpr size_t SIZE = HEADER_SIZE + sizeof(Payload);
		DataPacket(uint32_t device_ID, const double timestamp, const Payload &payload);
		void SetTimestamp(const double timestamp);
		void SetData(const Payload &payload);
		size_t Serialize(uint8_t *buf, size_t cap);
		void print();
		~DataPacket();
	};

    using BatteryPacket = DataPacket<Type::Battery, Structure::Battery_struct_t>;
    using PVPacket      = DataPacket<Type::PV,      Structure::PV_struct_t>;
    using WindPacket    = DataPacket<Type::Wind,    Structure::Wind_struct_t>;
    using ACPacket      = DataPacket<Type::AC3,     Structure::AC_struct_t>;
    using PMUPacket     = DataPacket<Type::PMU,     Structure::PMU_struct_t>;
}

inline void put32(uint8_t *p, uint32_t v){ 
	v = __builtin_bswap32(v);
	std::memcpy(p, &v, 4);
}

inline void put64(uint8_t *p, uint64_t v){
	v = __builtin_bswap64(v);
	std::memcpy(p, &v, 8);
}

inline void putf(uint8_t *p, float f){
	uint32_t u; std::memcpy(&u, &f, 4);
	put32(p, u);
}

inline void putd(uint8_t *p, double d) {
	uint64_t u; std::memcpy(&u, &d, 8);
	put64(p, u);
}