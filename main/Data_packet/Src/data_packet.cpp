#include "data_packet.h"

template <Packet::Type T, typename Payload>
Packet::DataPacket<T, Payload>::DataPacket(uint32_t device_ID, const double timestamp, const Payload &payload) 
: device_ID(device_ID), timestamp(timestamp), payload_(payload) {}

template <Packet::Type T, typename Payload>
void Packet::DataPacket<T, Payload>::SetTimestamp(const double timestamp){
    this->timestamp = timestamp;
}

template <Packet::Type T, typename Payload>
void Packet::DataPacket<T, Payload>::SetData(const Payload &payload){
    this->payload_ = payload;
}

template <Packet::Type T, typename Payload>
size_t Packet::DataPacket<T, Payload>::Serialize(uint8_t *buf, size_t cap) {
    if (buf == nullptr || cap < SIZE) return 0;

    buf[0] = static_cast<uint8_t>(T);
    put32(buf + 1, device_ID);
    putd(buf + 5, timestamp);

    const uint8_t *src = reinterpret_cast<const uint8_t *>(&payload_);
    for (size_t i = 0; i < sizeof(Payload) / sizeof(float); ++i) {
        float f;
        std::memcpy(&f, src + 4 * i, 4);
        putf(buf + HEADER_SIZE + 4 * i, f);
    }
    return SIZE;
}

template class Packet::DataPacket<Packet::Type::Battery, Packet::Structure::Battery_struct_t>;
template class Packet::DataPacket<Packet::Type::PV,      Packet::Structure::PV_struct_t>;
template class Packet::DataPacket<Packet::Type::Wind,    Packet::Structure::Wind_struct_t>;
template class Packet::DataPacket<Packet::Type::AC3,     Packet::Structure::AC_struct_t>;
template class Packet::DataPacket<Packet::Type::PMU,     Packet::Structure::PMU_struct_t>;

// Ai suggested this
// Compile-time proof the sizes match the schema (13 + 4N).
static_assert(Packet::BatteryPacket::SIZE == 25);
static_assert(Packet::PVPacket::SIZE      == 21);
static_assert(Packet::WindPacket::SIZE    == 37);
static_assert(Packet::ACPacket::SIZE      == 85);
static_assert(Packet::PMUPacket::SIZE     == 61);