#if !defined(KONEXIOS_MD5SUM_H_)
#define KONEXIOS_MD5SUM_H_

int md5sum(char *hash, const char *data, int len);

void md5_chunk_init();
void md5_chunk(const char *data, int len);
int md5_chunk_hash(char *hash);

#endif  // KONEXIOS_MD5SUM_H_
