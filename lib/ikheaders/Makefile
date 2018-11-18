# SPDX-License-Identifier: GPL-2.0
#
datafile_y = ikh_data.tbz
AFLAGS_ikheaders_data.o += -DIKH_IMAGE="$(obj)/$(datafile_y)"

obj-y := ikheaders_data.o
$(obj)/ikheaders_data.o: $(obj)/$(datafile_y)

# List of required headers and other files
ikh_file_list := include/
ikh_file_list += arch/$(ARCH)/Makefile
ikh_file_list += arch/$(ARCH)/include/
ikh_file_list += $(IKH_EXTRA)
ikh_file_list += scripts/
ikh_file_list += Makefile
ikh_file_list += Module.symvers
ifeq ($(CONFIG_STACK_VALIDATION), y)
ikh_file_list += $(objtree)/tools/objtool/objtool
endif

$(obj)/$(datafile_y): FORCE
	$(srctree)/$(src)/gen_ikh_data.sh $(obj)/$(datafile_y) $(ikh_file_list)
