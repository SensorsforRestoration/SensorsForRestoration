from influxdb_client_3 import (
  InfluxDBClient3, InfluxDBError, Point, WritePrecision,
  WriteOptions, write_client_options)

INTERVAL = 10

DEFAULT_WRITE_OPTIONS = WriteOptions(batch_size=500,
                                    flush_interval=10_000,
                                    jitter_interval=2_000,
                                    retry_interval=5_000,
                                    max_retries=5,
                                    max_retry_delay=30_000,
                                    exponential_base=2)

class Influx():
    def __init__(self, host, token, database, write_options=DEFAULT_WRITE_OPTIONS):
        self.host = host
        self.token = token
        self.database = database    
        self.wco = write_client_options(success_callback=success,
                          error_callback=error,
                          retry_callback=retry,
                          write_options=write_options)

    def upload(self, packet):
        points = []

        # We create a datapoint for every discrete depth
        # measurement, which there are 360 of that occur
        # every 10 seconds. Additionally, at the first timestamp
        # there is also a temperature measurement, and at the first
        # and middle timestamps there are salinity measurements.
        for i in range(len(packet["data"]["depth"])):
            timestamp = packet["timestamp"] + INTERVAL*i
            p = Point("sensor").tag("sensor_id", packet["sensor_id"]).field("depth", packet["data"]["depth"][i]).time(timestamp)

            if i == 0:
                p = p.field("temperature", packet["data"]["temperature"][0]).field("salinity", packet["data"]["salinity"][0])
            
            if i == 179:
                p = p.field("salinity", packet["data"]["salinity"][1])

            points.append(p)

        with InfluxDBClient3(host=self.host,
                        token=self.token,
                        database=self.database,
                        write_client_options=self.wco) as client:
            client.write(points, write_precision='s')

def success(self, data: str):
    print(f"Successfully wrote batch")

def error(self, data: str, exception: InfluxDBError):
    print(f"Failed writing batch: config: {self}, data: {data} due: {exception}")

def retry(self, data: str, exception: InfluxDBError):
    print(f"Failed retry writing batch: config: {self}, data: {data} retry: {exception}")