package com.intel.realsense.librealsense;

public enum ProductClass {
    ANY(0xff),
    ANY_INTEL(0xfe),
    NON_INTEL(0x01),
    D400(0x02),
    SR300(0x04),
    L500(0x08),
    T200(0x10),
    D400_RECOVERY(0x20),
    DEPTH(L500.value() | SR300.value() | D400.value()),
    TRACKING(T200.value());

    private final int mValue;

    private ProductClass(int value) { mValue = value; }
    public int value() { return mValue; }
}
