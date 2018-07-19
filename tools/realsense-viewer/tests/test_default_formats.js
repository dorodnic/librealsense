click_button(find_nth_element("Stereo Module", 2));
click_button(find_element("Infrared 1"));
click_button(find_element("Infrared 2"));

if (get_text(find_element("0 format")) != "Z16") fail("Default Depth format expected to be Z16");
if (get_text(find_element("1 format")) != "Y8") fail("Default Infrared format expected to be Y8");
if (get_text(find_element("2 format")) != "Y8") fail("Default Infrared format expected to be Y8");

click_button(find_element("RGB Camera"));
if (get_text(find_element("RGB Camera...format")) != "RGB8") fail("Default Color format expected to be RGB8");

pass("Hooray!");