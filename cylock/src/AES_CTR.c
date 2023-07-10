/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Sample to illustrate the usage of crypto APIs. The sample plaintext
 * and ciphertexts used for crosschecking are from TinyCrypt.
 */

#include <device.h>
#include <zephyr.h>
#include <string.h>
#include <crypto/cipher.h>
#include <stdio.h>

#ifdef CONFIG_CRYPTO_TINYCRYPT_SHIM
#define CRYPTO_DRV_NAME CONFIG_CRYPTO_TINYCRYPT_SHIM_DRV_NAME
#elif CONFIG_CRYPTO_MBEDTLS_SHIM
#define CRYPTO_DRV_NAME CONFIG_CRYPTO_MBEDTLS_SHIM_DRV_NAME
#elif DT_HAS_COMPAT_STATUS_OKAY(st_stm32_cryp)
#define CRYPTO_DRV_NAME DT_LABEL(DT_INST(0, st_stm32_cryp))
#elif DT_HAS_COMPAT_STATUS_OKAY(st_stm32_aes)
#define CRYPTO_DRV_NAME DT_LABEL(DT_INST(0, st_stm32_aes))
#elif CONFIG_CRYPTO_NRF_ECB
#define CRYPTO_DRV_NAME DT_LABEL(DT_INST(0, nordic_nrf_ecb))
#else
#error "You need to enable one crypto device"
#endif




uint32_t cap_flags;
const struct device * aes_dev;



static uint8_t key[16] = {
	0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88,
	0x09, 0xcf, 0x4f, 0x3c
};

int validate_hw_compatibility(const struct device *dev)
{
	uint32_t flags = 0U;

	flags = cipher_query_hwcaps(dev);
	if ((flags & CAP_RAW_KEY) == 0U) {
		printk("Please provision the key separately "
			"as the module doesnt support a raw key");
		return -1;
	}

	if ((flags & CAP_SYNC_OPS) == 0U) {
		printk("The app assumes sync semantics. "
		  "Please rewrite the app accordingly before proceeding");
		return -1;
	}

	if ((flags & CAP_SEPARATE_IO_BUFS) == 0U) {
		printk("The app assumes distinct IO buffers. "
		"Please rewrite the app accordingly before proceeding");
		return -1;
	}

	cap_flags = CAP_RAW_KEY | CAP_SYNC_OPS | CAP_SEPARATE_IO_BUFS;

	return 0;

}

void encryption_init (){

  aes_dev = device_get_binding(CRYPTO_DRV_NAME);
  if (!aes_dev) {
		printk("%s pseudo device not found\r\n", CRYPTO_DRV_NAME);
	}
   if (validate_hw_compatibility(aes_dev)) {
      printk("Incompatible h/w");
      return;
	}

}


void substring(char *destination, const char *source, int beg, int n)
{
	// extracts n characters from source string starting from beg index
	// and copy them into the destination string
	while (n > 0)
	{
		*destination = *(source + beg);
		destination++;
		source++;
		n--;
	}
	// null terminate destination string
	*destination = '\0';
}

uint8_t Two_Hex_Character_Equivalent_Int(char *Hex)
{   
    /**
     * hex2int
     * take a hex string and convert it to a 8bit number (max 2 hex digits)
    */ 
    uint8_t Equiv_Int = 0;
    while (*Hex) 
    {
        // get current character then increment
        char byte = *Hex++;
        // transform hex character to the 4bit equivalent number, using the ascii table indexes
        if (byte >= '0' && byte <= '9') byte = byte - '0';
        else if (byte >= 'a' && byte <='f') byte = byte - 'a' + 10;
        else if (byte >= 'A' && byte <='F') byte = byte - 'A' + 10;    
        // shift 4 to make space for new digit, and add the 4 bits of the new digit 
        Equiv_Int = (Equiv_Int << 4) | (byte & 0xF);
    }  
    return Equiv_Int;
}





void ctr_mode(const struct device *dev, char *S)
{

        uint8_t Message_To_Be_Encrypted[32] = {0};
        char tmp_String[3];
        uint8_t tmp_value;
        int j = 0;
        char s3[64];
 
        //Parse string and populate it in uint8_t array. 
        for (int i = 0; i<strlen(S);i=i+2)
        {
          substring(tmp_String, S, i,2);
          tmp_value = Two_Hex_Character_Equivalent_Int(tmp_String);
          Message_To_Be_Encrypted[j]=tmp_value;
          j++;    
        }

	uint8_t encrypted[32] = {0};
	uint8_t decrypted[32] = {0};

	struct cipher_ctx ini = {
		.keylen = sizeof(key),
		.key.bit_stream = key,
		.flags = cap_flags,
		/*  ivlen + ctrlen = keylen , so ctrlen is 128 - 96 = 32 bits */
		.mode_params.ctr_info.ctr_len = 32,
	};
	struct cipher_pkt encrypt = {
		.in_buf = Message_To_Be_Encrypted,
		.in_len = sizeof(Message_To_Be_Encrypted),
		.out_buf_max = sizeof(encrypted),
		.out_buf = encrypted,
	};
	struct cipher_pkt decrypt = {
		.in_buf = encrypted,
		.in_len = sizeof(encrypted),
		.out_buf = decrypted,
		.out_buf_max = sizeof(decrypted),
	};
	uint8_t iv[12] = {
		0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		0xf8, 0xf9, 0xfa, 0xfb
	};

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_CTR,
				 CRYPTO_CIPHER_OP_ENCRYPT)) {
		return;
	}

	if (cipher_ctr_op(&ini, &encrypt, iv)) {
		printk("CTR mode ENCRYPT - Failed");
                goto out;
	}

	cipher_free_session(dev, &ini);

	if (cipher_begin_session(dev, &ini, CRYPTO_CIPHER_ALGO_AES,
				 CRYPTO_CIPHER_MODE_CTR,
				 CRYPTO_CIPHER_OP_DECRYPT)) {
		return;
	}

	if (cipher_ctr_op(&ini, &decrypt, iv)) {
		printk("CTR mode DECRYPT - Failed");
                goto out;
	}

        /*
        int m;
        printk("Encrypted Payload is:\r\n");
        for (m=0;m<sizeof(encrypted);m++){
        printk("%02X",encrypted[m]);
        }
        printk("\r\n");

        int x;
        printk("Decrypted Payload is:\r\n");
        for (x=0;x<sizeof(decrypted);x++){
        printk("%02X",decrypted[x]);
        }
        printk("\r\n");
        */


        memset(S,0,sizeof(S));
        for (int i = 0;i<32;i++){
                  sprintf(s3,"%02x",encrypted[i]);
                  strcat(S,s3);
        } 
        //printk("Published is:\r\n%s\r\n",S);
        

        out:
	cipher_free_session(dev, &ini);
}

