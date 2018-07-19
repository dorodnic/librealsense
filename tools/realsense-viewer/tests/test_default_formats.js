e = get_elements();
for (i = 0; i < e.length; i++) {
    log(e[i]);
}

click_button(find_nth_element("Stereo Module", 2));
log(get_text(find_element("format")))