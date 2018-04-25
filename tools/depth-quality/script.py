from device_framework import *
#class software_camera:
#    def __init__(self, server):
#        return

import pyrealsense2 as rs
import numpy as np

def frame_to_numpy(frame):
    data = frame.as_frame().get_data()
    return np.asanyarray(data)

def generate_depth(left, right, backup):
    left = left.astype(float)
    right = right.astype(float)
    disparities = np.zeros(shape=(left.shape))
    w, h = right.shape
    for x in xrange(left.shape[0]):
        for y in xrange(left.shape[1]):
            ymax = min(w - 1, y + 16)
            m = abs(left[x][y] - right[x][y])
            for y2 in xrange(y, ymax):
                if (abs(left[x][y] - right[x][y2]) < m):
                    m = abs(left[x][y] - right[x][y2])
            d = 1000 / (m + 1);
            disparities[x][y] = d;
    return disparities.astype("uint16") 

class depth_from_stereo(software_camera):
    def __init__(self, server):
        software_camera.__init__(self, server)

    def update(self):
        frames = self.pipe.wait_for_frames()
        left = frame_to_numpy(frames.get_infrared_frame(0))
        right = frame_to_numpy(frames.get_infrared_frame(1))
        hardware_depth = frame_to_numpy(frames.get_depth_frame())
        depth = generate_depth(left, right, hardware_depth)

        self.upload_z(depth, 2)
        self.upload_y(left, 0)
        self.upload_y(right, 1)

    def start(self):
        self.pipe = rs.pipeline();
        cfg = rs.config();
        cfg.enable_stream(rs.stream.infrared, 1);
        cfg.enable_stream(rs.stream.infrared, 2);
        cfg.enable_stream(rs.stream.depth);
        self.pipe.start(cfg);
        print "Pipeline started!"

    def stop(self):
        self.pipe.stop();
        print "Pipeline stopped!"

def upload(buff):
    return

if __name__ == "__main__":
    cam = depth_from_stereo(0);
    cam.upload = upload
    cam.start()
    cam.update()
    cam.stop()
