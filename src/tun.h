#pragma once

#include <linux/if.h>
#include <linux/if_tun.h>

int tun_create(char *dev, int flags);
void tun_destroy(char *dev);
