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
import com.intel.realsense.librealsense.FwUpdateDevice;
import com.intel.realsense.librealsense.ProductClass;
import com.intel.realsense.librealsense.ProgressListener;
import com.intel.realsense.librealsense.RsContext;

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
                        d.enterToFwUpdateMode();
                    }
                }
            }
        });

        try(DeviceList dl = mRsContext.queryDevices(ProductClass.DEPTH)){
            if(dl.getDeviceCount() > 0){
                mFwUpdateButton.setVisibility(View.VISIBLE);
                printInfo();
            }
        }
        try(DeviceList dl = mRsContext.queryDevices(ProductClass.D400_RECOVERY)) {
            if(dl.getDeviceCount() > 0){
                mFwUpdateButton.setVisibility(View.GONE);
                Thread t = new Thread(new Runnable() {
                    @Override
                    public void run() {
                        updateFirmware(FirmwareUpdateActivity.this, R.raw.fw_d4xx);
                    }
                });
                t.start();
            }
        }
        try(DeviceList dl = mRsContext.queryDevices(ProductClass.SR300_RECOVERY)) {
            if(dl.getDeviceCount() > 0){
                Log.e(TAG, "SR300 FW update is not supported");
                Toast.makeText(this, "SR300 FW update is not supported", Toast.LENGTH_LONG).show();
                finish();
            }
        }
    }

    private void printInfo(){
        try(DeviceList dl = mRsContext.queryDevices(ProductClass.DEPTH)) {
            if(dl.getDeviceCount() == 0)
                return;
            try(Device device = dl.createDevice(0)){
                final String recFw = device.getInfo(CameraInfo.RECOMMENDED_FIRMWARE_VERSION);
                final String fw = device.getInfo(CameraInfo.FIRMWARE_VERSION);
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        TextView textView = findViewById(R.id.fwUpdateMainText);
                        textView.setText("The FW of the connected device is:\n " + fw +
                                "\n\nThe minimal recommended FW for this device is:\n " + recFw +
                                "\n\nClicking " + mFwUpdateButton.getText() + " will update to FW " + getString(R.string.d4xx_fw_version));
                    }
                });
            }
        }
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

    private synchronized void updateFirmware(Context context, int fwResId) {
        try {
            final byte[] bytes = readFwFile(context, fwResId);
            try (DeviceList dl = mRsContext.queryDevices(ProductClass.D400_RECOVERY)) {
                if (dl.getDeviceCount() == 0)
                    throw new Exception("device not found");
                try (Device d = dl.createDevice(0)) {
                    FwUpdateDevice fwud = d.as(FwUpdateDevice.class);
                    fwud.updateFw(bytes, new ProgressListener() {
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
                }
            }
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
