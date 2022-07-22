/* File generated by C:/ncs/v1.9.1/nrf/scripts/partition_manager_output.py, do not modify */
#ifndef PM_CONFIG_H__
#define PM_CONFIG_H__
#define PM_MCUBOOT_OFFSET 0x0
#define PM_MCUBOOT_ADDRESS 0x0
#define PM_MCUBOOT_END_ADDRESS 0xc000
#define PM_MCUBOOT_SIZE 0xc000
#define PM_MCUBOOT_NAME mcuboot
#define PM_MCUBOOT_ID 0
#define PM_mcuboot_ID PM_MCUBOOT_ID
#define PM_mcuboot_IS_ENABLED 1
#define PM_0_LABEL MCUBOOT
#define PM_MCUBOOT_DEV_NAME "NRF_FLASH_DRV_NAME"
#define PM_MCUBOOT_PAD_OFFSET 0xc000
#define PM_MCUBOOT_PAD_ADDRESS 0xc000
#define PM_MCUBOOT_PAD_END_ADDRESS 0xc200
#define PM_MCUBOOT_PAD_SIZE 0x200
#define PM_MCUBOOT_PAD_NAME mcuboot_pad
#define PM_MCUBOOT_PAD_ID 1
#define PM_mcuboot_pad_ID PM_MCUBOOT_PAD_ID
#define PM_mcuboot_pad_IS_ENABLED 1
#define PM_1_LABEL MCUBOOT_PAD
#define PM_MCUBOOT_PAD_DEV_NAME "NRF_FLASH_DRV_NAME"
#define PM_MCUBOOT_PRIMARY_OFFSET 0xc000
#define PM_MCUBOOT_PRIMARY_ADDRESS 0xc000
#define PM_MCUBOOT_PRIMARY_END_ADDRESS 0x85000
#define PM_MCUBOOT_PRIMARY_SIZE 0x79000
#define PM_MCUBOOT_PRIMARY_NAME mcuboot_primary
#define PM_MCUBOOT_PRIMARY_ID 2
#define PM_mcuboot_primary_ID PM_MCUBOOT_PRIMARY_ID
#define PM_mcuboot_primary_IS_ENABLED 1
#define PM_2_LABEL MCUBOOT_PRIMARY
#define PM_MCUBOOT_PRIMARY_DEV_NAME "NRF_FLASH_DRV_NAME"
#define PM_APP_OFFSET 0xc200
#define PM_APP_ADDRESS 0xc200
#define PM_APP_END_ADDRESS 0x85000
#define PM_APP_SIZE 0x78e00
#define PM_APP_NAME app
#define PM_APP_ID 3
#define PM_app_ID PM_APP_ID
#define PM_app_IS_ENABLED 1
#define PM_3_LABEL APP
#define PM_APP_DEV_NAME "NRF_FLASH_DRV_NAME"
#define PM_MCUBOOT_PRIMARY_APP_OFFSET 0xc200
#define PM_MCUBOOT_PRIMARY_APP_ADDRESS 0xc200
#define PM_MCUBOOT_PRIMARY_APP_END_ADDRESS 0x85000
#define PM_MCUBOOT_PRIMARY_APP_SIZE 0x78e00
#define PM_MCUBOOT_PRIMARY_APP_NAME mcuboot_primary_app
#define PM_MCUBOOT_PRIMARY_APP_ID 4
#define PM_mcuboot_primary_app_ID PM_MCUBOOT_PRIMARY_APP_ID
#define PM_mcuboot_primary_app_IS_ENABLED 1
#define PM_4_LABEL MCUBOOT_PRIMARY_APP
#define PM_MCUBOOT_PRIMARY_APP_DEV_NAME "NRF_FLASH_DRV_NAME"
#define PM_MCUBOOT_SECONDARY_OFFSET 0x85000
#define PM_MCUBOOT_SECONDARY_ADDRESS 0x85000
#define PM_MCUBOOT_SECONDARY_END_ADDRESS 0xfe000
#define PM_MCUBOOT_SECONDARY_SIZE 0x79000
#define PM_MCUBOOT_SECONDARY_NAME mcuboot_secondary
#define PM_MCUBOOT_SECONDARY_ID 5
#define PM_mcuboot_secondary_ID PM_MCUBOOT_SECONDARY_ID
#define PM_mcuboot_secondary_IS_ENABLED 1
#define PM_5_LABEL MCUBOOT_SECONDARY
#define PM_MCUBOOT_SECONDARY_DEV_NAME "NRF_FLASH_DRV_NAME"
#define PM_SETTINGS_STORAGE_OFFSET 0xfe000
#define PM_SETTINGS_STORAGE_ADDRESS 0xfe000
#define PM_SETTINGS_STORAGE_END_ADDRESS 0x100000
#define PM_SETTINGS_STORAGE_SIZE 0x2000
#define PM_SETTINGS_STORAGE_NAME settings_storage
#define PM_SETTINGS_STORAGE_ID 6
#define PM_settings_storage_ID PM_SETTINGS_STORAGE_ID
#define PM_settings_storage_IS_ENABLED 1
#define PM_6_LABEL SETTINGS_STORAGE
#define PM_SETTINGS_STORAGE_DEV_NAME "NRF_FLASH_DRV_NAME"
#define PM_SRAM_PRIMARY_OFFSET 0x0
#define PM_SRAM_PRIMARY_ADDRESS 0x20000000
#define PM_SRAM_PRIMARY_END_ADDRESS 0x20040000
#define PM_SRAM_PRIMARY_SIZE 0x40000
#define PM_SRAM_PRIMARY_NAME sram_primary
#define PM_NUM 7
#define PM_ALL_BY_SIZE "mcuboot_pad settings_storage mcuboot sram_primary app mcuboot_primary_app mcuboot_secondary mcuboot_primary"
#define PM_ADDRESS 0xc200
#define PM_SIZE 0x78e00
#define PM_SRAM_ADDRESS 0x20000000
#define PM_SRAM_SIZE 0x40000
#endif /* PM_CONFIG_H__ */