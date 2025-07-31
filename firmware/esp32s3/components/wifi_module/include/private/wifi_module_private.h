#pragma once
#ifndef WIFI_MODULE_PRIVATE_H
#define WIFI_MODULE_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_STA_CONNECTED_BIT BIT0
#define WIFI_STA_ERROR_BIT     BIT1

#define WIFI_ALL_BITS (WIFI_STA_CONNECTED_BIT | WIFI_STA_ERROR_BIT)

/*DHCP server option*/
#define DHCPS_OFFER_DNS 0x02

#ifdef __cplusplus
}
#endif

#endif