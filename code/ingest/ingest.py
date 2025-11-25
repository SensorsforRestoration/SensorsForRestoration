import sys
from reader import read_packet
from influx import Influx

host = "http://localhost:8181"
token = "apiv3_wxECbHnYZ7WnbDpkRrvijpdJ9HFCb4u68cu5hV1_Wg8PQWXudJqxoiw4UqbWwHEr-Bpa2xNZQF-amtOXX9WIfw"
database = "test"

client = Influx(host, token, database)

if len(sys.argv) != 2:
    print("One file argument required")

packet = read_packet(sys.argv[1])
client.upload(packet)