#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
COMPONENT_ADD_LDFLAGS=-l$(COMPONENT_NAME) 	\
	$(COMPONENT_PATH)/lib/libFLAC.a
