class software_camera: # don't modify this class
    def embedded(self):
        return False
    def __init__(self, server):
        return

import pyrealsense2 as rs
import cv2
import numpy as np
from matplotlib import pyplot as plt
from matplotlib import cm

def frame_to_numpy(frame):
    data = frame.as_frame().get_data()
    return np.asanyarray(data)

class depth_from_stereo(software_camera):
    def __init__(self, server):
        software_camera.__init__(self, server) 

    def generate_depth(self, left, right, backup):
        stereo = cv2.StereoBM_create(numDisparities=128, blockSize=101)
        disparity = stereo.compute(left, right)
        depth = np.zeros(shape=disparity.shape).astype(float)
        
        depth[disparity > 0] = (self.fx * self.baseline) / (self.units * 32 * disparity[disparity > 0])

        #print "Distance at 'inf' = {}".format(self.fx * self.baseline / 32)
        #print "Distance at 'close' = {}".format((self.fx * self.baseline) / (32 * 64))

        if self.embedded() == False:
            plt.imshow(depth, cmap=cm.jet)
            plt.show()
        return depth.astype("uint16")
        #return backup

    def update(self):
        frames = self.pipe.wait_for_frames()
        left = frame_to_numpy(frames.get_infrared_frame(1))
        right = frame_to_numpy(frames.get_infrared_frame(2))
        hardware_depth = frame_to_numpy(frames.get_depth_frame())
        depth = self.generate_depth(left, right, hardware_depth)

        self.upload_z(depth, 2)
        self.upload_z(hardware_depth, 3)
        self.upload_y(left, 0)
        self.upload_y(right, 1)

    def start(self):
        self.pipe = rs.pipeline()
        cfg = rs.config()
        cfg.enable_stream(rs.stream.infrared, 1)
        cfg.enable_stream(rs.stream.infrared, 2)
        cfg.enable_stream(rs.stream.depth)
        prof = self.pipe.start(cfg)
        ds = prof.get_stream(rs.stream.depth, 0)
        intr = ds.as_video_stream_profile().get_intrinsics()
        self.fx = intr.fx
        print "Focal Length = {}".format(self.fx)
        sensor = prof.get_device().query_sensors()[0];

        #sensor.set_option(rs.option.emitter_enabled, 0.0)
        #sensor.set_option(rs.option.gain, 58.0)
        #sensor.set_option(rs.option.exposure, 54620.0)
        
        self.baseline = sensor.get_option(rs.option.stereo_baseline)
        self.units = sensor.get_option(rs.option.depth_units)
        
        print "Baseline = {}".format(self.baseline)
        print "Pipeline started!"

    def stop(self):
        self.pipe.stop();
        print "Pipeline stopped!"

def upload(buff, idx):
    return

if __name__ == "__main__":
    cam = depth_from_stereo(0);
    cam.upload_z = upload
    cam.upload_y = upload
    cam.start()
    try:
        #while True:
        cam.update()
    except KeyboardInterrupt:
        cam.stop()
    cam.stop()
