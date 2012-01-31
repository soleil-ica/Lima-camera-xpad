from PyTango import *
import sys
import time



	
xpad_specif_server_name = "det/xpad/xpad.1"


try :

	#===============================================
	# Scan Server Proxy
	#===============================================
	print "==============================================="
	xpad_specif = DeviceProxy(xpad_specif_server_name)
	print "-> ping(xpad_specif) ",xpad_specif.ping()," us"
	
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
