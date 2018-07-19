#
# Copyright 2013, Broadcom Corporation
# All Rights Reserved.
#
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#

NAME := App_HTTPS_Client

$(NAME)_SOURCES := https_client.c

$(NAME)_COMPONENTS := protocols/http

GLOBAL_DEFINES :=

WIFI_CONFIG_DCT_H := wifi_config_dct.h