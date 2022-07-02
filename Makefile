CFLAGS := -fPIC -O0 -g -Wall -Werror
CC := gcc
VERSION := 0
NAME := iommu
SRCS = iommu_reset.c iommu_reg.c iommu_translate.c iommu_faults.c
SRCS_APP = test_app.c
OBJS = $(SRCS:.c=.o)

lib: lib$(NAME).so.$(VERSION)

lib$(NAME).so.$(VERSION): $(OBJS)
	$(CC) -shared -Wl,-soname,lib$(NAME).so.$(VERSION) $^ -o $@

OBJ_APP = $(SRCS_APP:.c=.o)
iommu: $(OBJ_APP)
	$(CC) -o $@ $^ $(CFLAGS) lib$(NAME).so.$(VERSION)

clean:
	$(RM) *.o *.so*
