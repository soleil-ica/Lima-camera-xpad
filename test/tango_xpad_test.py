from PyTango import *
import sys
import time


xpad_generic_device_name = "det/xpad/limaxpad-swing"	
xpad_specif_device_name = "det/xpad/xpad-swing"

print "==============================================="
xpad_generic = DeviceProxy(xpad_generic_device_name)
print "-> ping(xpad_generic) ",xpad_generic.ping()," us"
xpad_specif = DeviceProxy(xpad_specif_device_name)
print "-> ping(xpad_specif) ",xpad_specif.ping()," us"

###############################################################
def test_loop_1_image(number_of_iteration,exposure_time_ms):
	try :
		xpad_generic.exposureTime = exposure_time_ms
		for i in range (1,number_of_iteration+1):
			print "Snaping image %d out of %d" % (i,number_of_iteration)
			xpad_generic.Snap()
			wait_end_acquisition()
	
		# EXCEPTIONS	
	except DevFailed:
		exctype , value = sys.exc_info()[:2]
		print "Failed with exception !", exctype
		for err in value :
			print "------- ERROR ELEMENT -------"
			print "reason:" , err['reason']
			print "description:" , err["desc"]
			print "origin:" , err["origin"]
			print "severity:" , err["severity"]

###############################################################
def test_save_dacl():
	try :
		mod_number = 4;
		calib_id = 0;
		chip_id = 0;
		current_row = 0;
	
		argin = [mod_number,calib_id,chip_id,current_row,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34,34]
		xpad_specif.SaveConfigL(argin)
	
		argin = [mod_mask,calib_id]
		xpad_specif.LoadConfig(argin)
	
		
		# EXCEPTIONS	
	except DevFailed:
		exctype , value = sys.exc_info()[:2]
		print "Failed with exception !", exctype
		for err in value :
			print "------- ERROR ELEMENT -------"
			print "reason:" , err['reason']
			print "description:" , err["desc"]
			print "origin:" , err["origin"]
			print "severity:" , err["severity"]

###############################################################
def wait_end_acquisition():
	while (xpad_generic.State() == PyTango.DevState.RUNNING)
		time.sleep(0.001)
