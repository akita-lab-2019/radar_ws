BUILD_ALL += $(OUT_DIR)/example_detector_distance_basic_rpi_xc112_r2b_xr112_r2b_a111_r2c

$(OUT_DIR)/example_detector_distance_basic_rpi_xc112_r2b_xr112_r2b_a111_r2c : \
					$(OUT_OBJ_DIR)/example_detector_distance_basic.o \
					$(OUT_OBJ_DIR)/acc_detector_distance_basic.o \
					libacconeer.a \
					libacconeer_sensor.a \
					libcustomer.a \
					libacc_service.a \
					$(OUT_OBJ_DIR)/acc_board_rpi_xc112_r2b_xr112_r2b.o
	@echo "    Linking $(notdir $@)"
	@mkdir -p $(OUT_DIR)
	@$(LINK.o) -Wl,--start-group $^ -Wl,--end-group $(LOADLIBES) $(LDLIBS) -o $@
