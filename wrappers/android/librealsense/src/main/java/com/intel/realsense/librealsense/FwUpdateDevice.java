package com.intel.realsense.librealsense;

public class FwUpdateDevice extends Device {
    private ProgressListener mListener;
    public void updateFw(byte[] image){
        updateFw(image, null);
    }

    public synchronized void updateFw(byte[] image, ProgressListener listener){
        mListener = listener;
        nUpdateFw(mHandle, image);
    }

    public FwUpdateDevice(long handle){
        super(handle);
    }

    void onProgress(float progress){
        mListener.onProgress(progress);
    }
    private native void nUpdateFw(long handle, byte[] image);

}
