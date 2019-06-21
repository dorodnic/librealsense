package com.intel.realsense.camera;

import android.content.Context;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import com.intel.realsense.librealsense.CameraInfo;
import com.intel.realsense.librealsense.Device;
import com.intel.realsense.librealsense.DeviceList;
import com.intel.realsense.librealsense.ProductClass;
import com.intel.realsense.librealsense.ProgressListener;
import com.intel.realsense.librealsense.RsContext;
import com.intel.realsense.librealsense.Updatable;
import com.intel.realsense.librealsense.UpdateDevice;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

public class FirmwareUpdateActivity extends AppCompatActivity {
    private static final String TAG = "librs fwupdate";
    private Button mFwUpdateButton;
    private RsContext mRsContext = new RsContext();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_firmware_update);

        mFwUpdateButton = findViewById(R.id.startFwUpdateButton);
        mFwUpdateButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                try(DeviceList dl = mRsContext.queryDevices(ProductClass.DEPTH)){
                    try(Device d = dl.createDevice(0)){
                        if(d instanceof Updatable)
                            d.as(Updatable.class).enterUpdateState();
                        else
                            throw new RuntimeException("request to update a non updatable device");
                    }
                }
            }
        });

        try(DeviceList dl = mRsContext.queryDevices()){
            if(dl.getDeviceCount() == 0)
                return;
            try(Device device = dl.createDevice(0)){
                if(device instanceof UpdateDevice){
                    int fw_image = getFwImageId(device);
                    tryUpdate(fw_image);
                }
                else{
                    printInfo(device);
                }
            }
        }
    }

    private void tryUpdate(final int fw_image){
        try{
            mFwUpdateButton.setVisibility(View.GONE);
            Thread t = new Thread(new Runnable() {
                @Override
                public void run() {
                    try(DeviceList dl = mRsContext.queryDevices()){
                        if(dl.getDeviceCount() == 0)
                            return;
                        try(Device device = dl.createDevice(0)){
                            if(device instanceof UpdateDevice){
                                UpdateDevice fwud = device.as(UpdateDevice.class);
                                updateFirmware(fwud, FirmwareUpdateActivity.this, fw_image);
                            }
                        }
                    }
                }
            });
            t.start();
        }
        catch(Exception e){
            Log.e(TAG, e.getMessage());
            Toast.makeText(this, e.getMessage(), Toast.LENGTH_LONG).show();
            finish();
        }
    }

    private int getFwImageId(Device device){
        if(device.supportsInfo(CameraInfo.PRODUCT_LINE)){
            String pl = device.getInfo(CameraInfo.PRODUCT_LINE);
            if(pl.equals("D400"))
                return R.raw.fw_d4xx;
            if(pl.equals("SR300"))
                return R.raw.fw_sr3xx;
        }
        throw new RuntimeException("FW update is not supported for the connected device");
    }

    private void printInfo(Device device){
        if(device instanceof Updatable)
            mFwUpdateButton.setVisibility(View.VISIBLE);
        final String buttonString = (device instanceof Updatable)?
                "\n\nClicking " + mFwUpdateButton.getText() + " will update to FW " + getString(R.string.d4xx_fw_version) :
                "";
        final String recFw = device.getInfo(CameraInfo.RECOMMENDED_FIRMWARE_VERSION);
        final String fw = device.getInfo(CameraInfo.FIRMWARE_VERSION);
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                TextView textView = findViewById(R.id.fwUpdateMainText);
                textView.setText("The FW of the connected device is:\n " + fw +
                        "\n\nThe minimal recommended FW for this device is:\n " + recFw + buttonString);
            }
        });
    }

    public static byte[] readFwFile(Context context, int fwResId) throws IOException {
        InputStream in = context.getResources().openRawResource(fwResId);
        int length = in.available();
        ByteBuffer buff = ByteBuffer.allocateDirect(length);
        int len = in.read(buff.array(),0, buff.capacity());
        in.close();
        byte[] rv = new byte[len];
        buff.get(rv, 0, len);
        return buff.array();
    }

    private synchronized void updateFirmware(UpdateDevice device,Context context, int fwResId) {
        try {
            final byte[] bytes = readFwFile(context, fwResId);
            device.update(bytes, new ProgressListener() {
                @Override
                public void onProgress(final float progress) {
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            TextView tv = findViewById(R.id.fwUpdateMainText);
                            tv.setText("FW update progress: " + (int) (progress * 100) + "[%]");
                        }
                    });
                }
            });
            Log.i(TAG, "Firmware update process finished successfully");
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    Toast.makeText(FirmwareUpdateActivity.this, "Firmware update process finished successfully", Toast.LENGTH_LONG).show();
                }
            });
        } catch (Exception e) {
            Log.e(TAG, "Firmware update process failed, error: " + e.getMessage());
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    Toast.makeText(FirmwareUpdateActivity.this, "Failed to set streaming configuration ", Toast.LENGTH_LONG).show();
                    TextView textView = findViewById(R.id.fwUpdateMainText);
                    textView.setText("FW Update Failed");
                }
            });
        }
    }
}
