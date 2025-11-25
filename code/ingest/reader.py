import struct

PACKET_SIZE = 1463

fmt = (
    "<"     # little endian
    "H"     # sequence_id
    "B"     # packet_num
    "H"     # total
    "I"     # timestamp
    "H"     # sensor_id
    "360f"  # depth
    "1f"    # temperature
    "2f"    # salinity
)

packet_struct = struct.Struct(fmt)

def read_packet(path):
    with open(path, "rb") as f:
        data = f.read(PACKET_SIZE)

    unpacked = packet_struct.unpack(data)

    return {
        "sequence_id": unpacked[0],
        "packet_num":  unpacked[1],
        "total":       unpacked[2],
        "timestamp":   unpacked[3],
        "sensor_id":   unpacked[4],
        "data": {
            "depth":       unpacked[5:5+360],
            "temperature": unpacked[5+360:5+361],
            "salinity":    unpacked[5+361:5+363],
        }
    }