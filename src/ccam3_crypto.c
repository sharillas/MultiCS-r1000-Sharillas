#include "common.h"

// ============================================================
// CCcam 3 crypto (pura C, sem OpenSSL)
// ============================================================

extern uint8_t fastrnd2(void); // definida em main.c (fallback random)

// ---------- SHA1 ----------
static uint32_t rol32(uint32_t v, int n) { return (v<<n) | (v>>(32-n)); }

static void sha1_block(uint32_t h[5], const uint8_t blk[64])
{
	uint32_t w[80];
	uint32_t a, b, c, d, e;
	int t;
	for (t=0; t<16; t++)
		w[t] = ((uint32_t)blk[t*4]<<24)|((uint32_t)blk[t*4+1]<<16)|((uint32_t)blk[t*4+2]<<8)|(uint32_t)blk[t*4+3];
	for (t=16; t<80; t++) w[t] = rol32(w[t-3]^w[t-8]^w[t-14]^w[t-16], 1);
	a=h[0]; b=h[1]; c=h[2]; d=h[3]; e=h[4];
	for (t=0; t<80; t++) {
		uint32_t f, k;
		if (t<20) { f=(b&c)|((~b)&d); k=0x5A827999; }
		else if (t<40) { f=b^c^d; k=0x6ED9EBA1; }
		else if (t<60) { f=(b&c)|(b&d)|(c&d); k=0x8F1BBCDC; }
		else { f=b^c^d; k=0xCA62C1D6; }
		uint32_t tmp = rol32(a,5)+f+e+k+w[t];
		e=d; d=c; c=rol32(b,30); b=a; a=tmp;
	}
	h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e;
}

void ccam3_sha1(const uint8_t *data, size_t len, uint8_t out[20])
{
	uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
	uint64_t bitlen = (uint64_t)len * 8;
	uint8_t buf[128];
	size_t i;

	for (i=0; i+64<=len; i+=64) sha1_block(h, data+i);

	size_t rem = len - i;
	memcpy(buf, data+i, rem);
	buf[rem++] = 0x80;
	if (rem > 56) {
		while (rem < 64) buf[rem++] = 0;
		sha1_block(h, buf);
		rem = 0;
	}
	while (rem < 56) buf[rem++] = 0;
	for (i=0; i<8; i++) buf[56+i] = (uint8_t)(bitlen >> (56-i*8));
	sha1_block(h, buf);

	for (i=0; i<5; i++) {
		out[i*4]   = (uint8_t)(h[i]>>24);
		out[i*4+1] = (uint8_t)(h[i]>>16);
		out[i*4+2] = (uint8_t)(h[i]>>8);
		out[i*4+3] = (uint8_t)(h[i]);
	}
}

// ---------- SHA256 ----------
static const uint32_t K256[64] = {
	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static uint32_t ror32(uint32_t v, int n) { return (v>>n) | (v<<(32-n)); }

static void sha256_blocks(uint32_t h[8], const uint8_t *p, size_t nblocks)
{
	uint32_t w[64];
	size_t b;
	for (b=0; b<nblocks; b++) {
		const uint8_t *blk = p + b*64;
		int t;
		for (t=0; t<16; t++)
			w[t] = ((uint32_t)blk[t*4]<<24)|((uint32_t)blk[t*4+1]<<16)|((uint32_t)blk[t*4+2]<<8)|(uint32_t)blk[t*4+3];
		for (t=16; t<64; t++) {
			uint32_t s0 = ror32(w[t-15],7)^ror32(w[t-15],18)^(w[t-15]>>3);
			uint32_t s1 = ror32(w[t-2],17)^ror32(w[t-2],19)^(w[t-2]>>10);
			w[t] = w[t-16]+s0+w[t-7]+s1;
		}
		uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
		for (t=0; t<64; t++) {
			uint32_t S1 = ror32(e,6)^ror32(e,11)^ror32(e,25);
			uint32_t ch = (e&f)^((~e)&g);
			uint32_t tmp1 = hh+S1+ch+K256[t]+w[t];
			uint32_t S0 = ror32(a,2)^ror32(a,13)^ror32(a,22);
			uint32_t maj = (a&b)^(a&c)^(b&c);
			uint32_t tmp2 = S0+maj;
			hh=g; g=f; f=e; e=d+tmp1; d=c; c=b; b=a; a=tmp1+tmp2;
		}
		h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
	}
}

void ccam3_sha256(const uint8_t *data, size_t len, uint8_t out[32])
{
	uint32_t h[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
	                  0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
	uint64_t bitlen = (uint64_t)len * 8;
	uint8_t buf[128];
	size_t nblocks = len/64;
	sha256_blocks(h, data, nblocks);
	size_t rem = len - nblocks*64;
	memcpy(buf, data+nblocks*64, rem);
	buf[rem++] = 0x80;
	if (rem > 56) { while (rem<64) buf[rem++]=0; sha256_blocks(h, buf, 1); rem=0; }
	while (rem < 56) buf[rem++] = 0;
	{
		int i;
		for (i=0; i<8; i++) buf[56+i] = (uint8_t)(bitlen >> (56-i*8));
	}
	sha256_blocks(h, buf, 1);
	{
		int i;
		for (i=0; i<8; i++) {
			out[i*4]   = (uint8_t)(h[i]>>24);
			out[i*4+1] = (uint8_t)(h[i]>>16);
			out[i*4+2] = (uint8_t)(h[i]>>8);
			out[i*4+3] = (uint8_t)(h[i]);
		}
	}
}

// ---------- HMAC-SHA256 ----------
void ccam3_hmac_sha256(const uint8_t *key, size_t keylen,
                       const uint8_t *data, size_t datalen, uint8_t out[32])
{
	uint8_t k[64];
	uint8_t ipad[64], opad[64];
	uint8_t ih[32];
	size_t i;
	memset(k, 0, 64);
	if (keylen > 64) { ccam3_sha256(key, keylen, ih); memcpy(k, ih, 32); }
	else memcpy(k, key, keylen);
	for (i=0; i<64; i++) { ipad[i] = k[i]^0x36; opad[i] = k[i]^0x5c; }
	{
		uint8_t *tmp = malloc(64+datalen);
		memcpy(tmp, ipad, 64);
		memcpy(tmp+64, data, datalen);
		ccam3_sha256(tmp, 64+datalen, ih);
		free(tmp);
	}
	{
		uint8_t *tmp = malloc(64+32);
		memcpy(tmp, opad, 64);
		memcpy(tmp+64, ih, 32);
		ccam3_sha256(tmp, 64+32, out);
		free(tmp);
	}
}

// ---------- PBKDF2-HMAC-SHA256 ----------
void ccam3_pbkdf2_sha256(const char *pass, size_t passlen,
                         const uint8_t *salt, size_t saltlen,
                         int iterations, uint8_t *out, size_t outlen)
{
	uint8_t u[32], t[32];
	uint32_t block = 1;
	size_t done = 0;
	while (done < outlen) {
		size_t n = (outlen-done) < 32 ? (outlen-done) : 32;
		uint8_t *tmp = malloc(saltlen+4);
		memcpy(tmp, salt, saltlen);
		tmp[saltlen]   = (uint8_t)(block>>24);
		tmp[saltlen+1] = (uint8_t)(block>>16);
		tmp[saltlen+2] = (uint8_t)(block>>8);
		tmp[saltlen+3] = (uint8_t)(block);
		ccam3_hmac_sha256((const uint8_t*)pass, passlen, tmp, saltlen+4, u);
		free(tmp);
		memcpy(t, u, 32);
		{
			int i;
			for (i=1; i<iterations; i++) {
				ccam3_hmac_sha256((const uint8_t*)pass, passlen, u, 32, u);
				int j;
				for (j=0; j<32; j++) t[j] ^= u[j];
			}
		}
		memcpy(out+done, t, n);
		done += n;
		block++;
	}
}

// ---------- RC4 ----------
void ccam3_rc4(const uint8_t *key, size_t keylen, uint8_t *data, size_t len)
{
	uint8_t S[256];
	int i, j = 0;
	for (i=0; i<256; i++) S[i] = (uint8_t)i;
	for (i=0; i<256; i++) {
		j = (j + S[i] + key[i % keylen]) & 0xff;
		uint8_t t = S[i]; S[i] = S[j]; S[j] = t;
	}
	i = j = 0;
	size_t k;
	for (k=0; k<len; k++) {
		i = (i+1)&0xff;
		j = (j+S[i])&0xff;
		uint8_t t = S[i]; S[i] = S[j]; S[j] = t;
		data[k] ^= S[(S[i]+S[j])&0xff];
	}
}

// ---------- AES-256 ----------
typedef struct {
	uint32_t rk[60]; // 15 round keys x 4 words
} ccam3_aes_key;

static const uint8_t SBOX[256] = {
0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};
static const uint8_t RSBOX[256] = {
0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
};

static uint8_t xtime8(uint8_t x) { return (uint8_t)((x<<1) ^ ((x>>7) ? 0x1b : 0)); }
static uint8_t gm9(uint8_t x) { uint8_t t=xtime8(xtime8(xtime8(x))); return t^x; }
static uint8_t gm11(uint8_t x) { uint8_t t=xtime8(xtime8(xtime8(x))); return t^xtime8(x)^x; }
static uint8_t gm13(uint8_t x) { uint8_t t=xtime8(xtime8(xtime8(x))); return t^xtime8(xtime8(x))^x; }
static uint8_t gm14(uint8_t x) { uint8_t t=xtime8(xtime8(xtime8(x))); return t^xtime8(xtime8(x))^xtime8(x); }

static void aes256_keyschedule(const uint8_t key[32], ccam3_aes_key *ks)
{
	int i;
	for (i=0; i<8; i++)
		ks->rk[i] = ((uint32_t)key[i*4]<<24)|((uint32_t)key[i*4+1]<<16)|((uint32_t)key[i*4+2]<<8)|(uint32_t)key[i*4+3];
	for (i=8; i<60; i++) {
		uint32_t t = ks->rk[i-1];
		if ((i%8)==0) {
			t = rol32(t,8);
			t = ((uint32_t)SBOX[(t>>24)&0xff]<<24)|((uint32_t)SBOX[(t>>16)&0xff]<<16)|
			    ((uint32_t)SBOX[(t>>8)&0xff]<<8)|((uint32_t)SBOX[t&0xff]);
			t ^= (uint32_t)(1 << (((i/8)-1) & 7)) << 24; // Rcon 2^(i/8-1)
		}
		else if ((i%8)==4) {
			t = ((uint32_t)SBOX[(t>>24)&0xff]<<24)|((uint32_t)SBOX[(t>>16)&0xff]<<16)|
			    ((uint32_t)SBOX[(t>>8)&0xff]<<8)|((uint32_t)SBOX[t&0xff]);
		}
		ks->rk[i] = ks->rk[i-8] ^ t;
	}
}

static void aes256_encrypt_block(const ccam3_aes_key *ks, const uint8_t in[16], uint8_t out[16])
{
	uint8_t s[16];
	int r, c;
	for (c=0; c<4; c++) {
		uint32_t w = ((uint32_t)in[4*c]<<24)|((uint32_t)in[4*c+1]<<16)|((uint32_t)in[4*c+2]<<8)|(uint32_t)in[4*c+3];
		w ^= ks->rk[c];
		s[4*c]   = (uint8_t)(w>>24);
		s[4*c+1] = (uint8_t)(w>>16);
		s[4*c+2] = (uint8_t)(w>>8);
		s[4*c+3] = (uint8_t)(w);
	}
	for (r=1; r<14; r++) {
		uint8_t t[16];
		for (c=0; c<16; c++) t[c] = SBOX[s[c]];
		// shift rows
		s[0]=t[0]; s[1]=t[5]; s[2]=t[10]; s[3]=t[15];
		s[4]=t[4]; s[5]=t[9]; s[6]=t[14]; s[7]=t[3];
		s[8]=t[8]; s[9]=t[13]; s[10]=t[2]; s[11]=t[7];
		s[12]=t[12]; s[13]=t[1]; s[14]=t[6]; s[15]=t[11];
		// mix columns
		for (c=0; c<4; c++) {
			uint8_t a0=s[4*c], a1=s[4*c+1], a2=s[4*c+2], a3=s[4*c+3];
			s[4*c]   = xtime8(a0)^xtime8(a1)^a1^a2^a3;
			s[4*c+1] = a0^xtime8(a1)^xtime8(a2)^a2^a3;
			s[4*c+2] = a0^a1^xtime8(a2)^xtime8(a3)^a3;
			s[4*c+3] = xtime8(a0)^a0^a1^a2^xtime8(a3);
		}
		// add round key
		for (c=0; c<4; c++) {
			uint32_t w = ks->rk[4*r+c];
			s[4*c]   ^= (uint8_t)(w>>24);
			s[4*c+1] ^= (uint8_t)(w>>16);
			s[4*c+2] ^= (uint8_t)(w>>8);
			s[4*c+3] ^= (uint8_t)(w);
		}
	}
	// final round (no mix)
	{
		uint8_t t[16];
		for (c=0; c<16; c++) t[c] = SBOX[s[c]];
		s[0]=t[0]; s[1]=t[5]; s[2]=t[10]; s[3]=t[15];
		s[4]=t[4]; s[5]=t[9]; s[6]=t[14]; s[7]=t[3];
		s[8]=t[8]; s[9]=t[13]; s[10]=t[2]; s[11]=t[7];
		s[12]=t[12]; s[13]=t[1]; s[14]=t[6]; s[15]=t[11];
	}
	for (c=0; c<4; c++) {
		uint32_t w = ks->rk[56+c];
		out[4*c]   = (uint8_t)(s[4*c]   ^ (w>>24));
		out[4*c+1] = (uint8_t)(s[4*c+1] ^ (w>>16));
		out[4*c+2] = (uint8_t)(s[4*c+2] ^ (w>>8));
		out[4*c+3] = (uint8_t)(s[4*c+3] ^ (w));
	}
}

static void aes256_decrypt_block(const ccam3_aes_key *ks, const uint8_t in[16], uint8_t out[16])
{
	uint8_t s[16];
	int r, c;
	for (c=0; c<4; c++) {
		uint32_t w = ks->rk[56+c];
		s[4*c]   = (uint8_t)(in[4*c]   ^ (w>>24));
		s[4*c+1] = (uint8_t)(in[4*c+1] ^ (w>>16));
		s[4*c+2] = (uint8_t)(in[4*c+2] ^ (w>>8));
		s[4*c+3] = (uint8_t)(in[4*c+3] ^ (w));
	}
	for (r=13; r>=1; r--) {
		uint8_t t[16];
		// inv shift rows
		t[0]=s[0]; t[5]=s[1]; t[10]=s[2]; t[15]=s[3];
		t[4]=s[4]; t[9]=s[5]; t[14]=s[6]; t[3]=s[7];
		t[8]=s[8]; t[13]=s[9]; t[2]=s[10]; t[7]=s[11];
		t[12]=s[12]; t[1]=s[13]; t[6]=s[14]; t[11]=s[15];
		for (c=0; c<16; c++) s[c] = RSBOX[t[c]];
		// add round key
		for (c=0; c<4; c++) {
			uint32_t w = ks->rk[4*r+c];
			s[4*c]   ^= (uint8_t)(w>>24);
			s[4*c+1] ^= (uint8_t)(w>>16);
			s[4*c+2] ^= (uint8_t)(w>>8);
			s[4*c+3] ^= (uint8_t)(w);
		}
		// inv mix columns (9,11,13,14)
		for (c=0; c<4; c++) {
			uint8_t a0=s[4*c], a1=s[4*c+1], a2=s[4*c+2], a3=s[4*c+3];
			s[4*c]   = gm14(a0)^gm11(a1)^gm13(a2)^gm9(a3);
			s[4*c+1] = gm9(a0)^gm14(a1)^gm11(a2)^gm13(a3);
			s[4*c+2] = gm13(a0)^gm9(a1)^gm14(a2)^gm11(a3);
			s[4*c+3] = gm11(a0)^gm13(a1)^gm9(a2)^gm14(a3);
		}
	}
	// inv shift rows final + inv sbox + key 0
	{
		uint8_t t[16];
		t[0]=s[0]; t[5]=s[1]; t[10]=s[2]; t[15]=s[3];
		t[4]=s[4]; t[9]=s[5]; t[14]=s[6]; t[3]=s[7];
		t[8]=s[8]; t[13]=s[9]; t[2]=s[10]; t[7]=s[11];
		t[12]=s[12]; t[1]=s[13]; t[6]=s[14]; t[11]=s[15];
		for (c=0; c<16; c++) s[c] = RSBOX[t[c]];
	}
	for (c=0; c<4; c++) {
		uint32_t w = ks->rk[c];
		out[4*c]   = (uint8_t)(s[4*c]   ^ (w>>24));
		out[4*c+1] = (uint8_t)(s[4*c+1] ^ (w>>16));
		out[4*c+2] = (uint8_t)(s[4*c+2] ^ (w>>8));
		out[4*c+3] = (uint8_t)(s[4*c+3] ^ (w));
	}
}

void ccam3_aes256_ecb(const uint8_t key[32], uint8_t *data, size_t len, int encrypt)
{
	ccam3_aes_key ks;
	aes256_keyschedule(key, &ks);
	size_t off;
	for (off=0; off+16<=len; off+=16) {
		if (encrypt) aes256_encrypt_block(&ks, data+off, data+off);
		else aes256_decrypt_block(&ks, data+off, data+off);
	}
}

// ---------- AES-256-GCM (sem AAD) ----------
static void gf128_mul(uint8_t y[16], const uint8_t h[16], const uint8_t x[16])
{
	// Z = Y + X*H em GF(2^128), polinomio x^128+x^7+x^2+x+1
	uint8_t z[16];
	uint8_t v[16];
	int i, j, k;
	memset(z, 0, 16);
	memcpy(v, h, 16);
	for (i=0; i<16; i++) {
		for (j=0; j<8; j++) {
			if (x[i] & (0x80>>j)) {
				for (k=0; k<16; k++) z[k] ^= v[k];
			}
			uint8_t carry = (v[0] & 0x80) ? 1 : 0;
			for (k=0; k<15; k++) v[k] = (uint8_t)((v[k]<<1) | (v[k+1]>>7));
			v[15] = (uint8_t)(v[15]<<1);
			if (carry) v[15] ^= 0xe1;
		}
	}
	for (i=0; i<16; i++) y[i] ^= z[i];
}

static void gcm_ghash(const uint8_t h[16], const uint8_t *ct, size_t ctlen, uint8_t out[16])
{
	memset(out, 0, 16);
	size_t nblocks = ctlen/16;
	size_t i;
	for (i=0; i<nblocks; i++) gf128_mul(out, h, ct+i*16);
	size_t rem = ctlen - nblocks*16;
	if (rem) {
		uint8_t last[16];
		memset(last, 0, 16);
		memcpy(last, ct+nblocks*16, rem);
		gf128_mul(out, h, last);
	}
	// bloco de comprimentos: len(ct)*8 (64 bits BE) || 0 (len aad = 0)
	uint8_t lblock[16];
	uint64_t bits = (uint64_t)ctlen * 8;
	memset(lblock, 0, 16);
	for (i=0; i<8; i++) lblock[8+i] = (uint8_t)(bits >> (56-i*8));
	gf128_mul(out, h, lblock);
}

static void gcm_ctr(const ccam3_aes_key *ks, uint8_t ctr[16], uint8_t *data, size_t len)
{
	uint8_t keystream[16];
	size_t off = 0;
	while (off < len) {
		aes256_encrypt_block(ks, ctr, keystream);
		size_t n = (len-off) < 16 ? (len-off) : 16;
		size_t i;
		for (i=0; i<n; i++) data[off+i] ^= keystream[i];
		off += n;
		// increment counter (32 bits finais)
		int k;
		for (k=15; k>=12; k--) {
			ctr[k]++;
			if (ctr[k]) break;
		}
	}
}

void ccam3_aes256_gcm_encrypt(const uint8_t key[32], const uint8_t iv[12],
                              const uint8_t *pt, size_t ptlen,
                              uint8_t *ct, uint8_t tag[16])
{
	ccam3_aes_key ks;
	aes256_keyschedule(key, &ks);
	uint8_t H[16];
	memset(H, 0, 16);
	aes256_encrypt_block(&ks, H, H);
	uint8_t J0[16];
	memset(J0, 0, 16);
	memcpy(J0, iv, 12);
	J0[15] = 0x01;
	memcpy(ct, pt, ptlen);
	uint8_t ctr[16];
	memcpy(ctr, J0, 16);
	gcm_ctr(&ks, ctr, ct, ptlen);
	uint8_t s[16];
	gcm_ghash(H, ct, ptlen, s);
	uint8_t ej0[16];
	aes256_encrypt_block(&ks, J0, ej0);
	int i;
	for (i=0; i<16; i++) tag[i] = s[i] ^ ej0[i];
}

int ccam3_aes256_gcm_decrypt(const uint8_t key[32], const uint8_t iv[12],
                             const uint8_t *ct, size_t ctlen,
                             const uint8_t tag[16], uint8_t *pt)
{
	ccam3_aes_key ks;
	aes256_keyschedule(key, &ks);
	uint8_t H[16];
	memset(H, 0, 16);
	aes256_encrypt_block(&ks, H, H);
	uint8_t J0[16];
	memset(J0, 0, 16);
	memcpy(J0, iv, 12);
	J0[15] = 0x01;
	uint8_t s[16];
	gcm_ghash(H, ct, ctlen, s);
	uint8_t ej0[16];
	aes256_encrypt_block(&ks, J0, ej0);
	int i;
	for (i=0; i<16; i++) if ((s[i]^ej0[i]) != tag[i]) return -1;
	memcpy(pt, ct, ctlen);
	uint8_t ctr[16];
	memcpy(ctr, J0, 16);
	gcm_ctr(&ks, ctr, pt, ctlen);
	return 0;
}

// ---------- random ----------
void ccam3_randbytes(uint8_t *buf, size_t len)
{
	static int urandom_fd = -1;
	size_t got = 0;
	if (urandom_fd < 0) urandom_fd = open("/dev/urandom", O_RDONLY);
	if (urandom_fd >= 0) {
		while (got < len) {
			ssize_t n = read(urandom_fd, buf+got, len-got);
			if (n <= 0) break;
			got += (size_t)n;
		}
	}
	while (got < len) buf[got++] = (uint8_t)(fastrnd2() & 0xff);
}
