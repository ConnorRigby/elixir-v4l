priv/webcam_port: src/webcam_port.c
	$(CC) src/webcam_port.c -o priv/webcam_port -L${ERL_EI_LIBDIR} -I${ERL_EI_INCLUDE_DIR} -lei