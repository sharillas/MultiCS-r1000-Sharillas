
inline int checksumDCW(uint8_t *data);
inline int isnullDCW(uint8_t *data);
inline int isbadDCW(uint8_t *data);
int acceptDCW(uint8_t *data, int isnanoe0);
int acceptDCWnonblockCRC(uint8_t *data);
// FILTRO EMBUTIDO (nao configuravel): fake cw com ultimo byte XOR 0xF0
inline int isfakecw_xorF0(uint8_t *data);
inline int acceptDCW_nanoe0(uint8_t *data);

int similarcw( uint8_t *cw1, uint8_t *cw2 );
// for nds
int ishalfnulledcw( uint8_t dcw[16] );

