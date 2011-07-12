/**
 * 数据保存到磁盘
 * @file dumpfile.c
 * @author zhaowei
 * @ingroup memlink
 * @{
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <event.h>
#include <evutil.h>
#include <sys/time.h>
#include "myconfig.h"
#include "logfile.h"
#include "dumpfile.h"
#include "utils.h"
#include "common.h"
#include "datablock.h"
#include "runtime.h"

/**
 * Creates a dump file from the hash table. The old dump file is replaced by 
 * the new dump file. Sync log is also rotated.
 * format:
 * ---------------------------------------------------------------------
 * | dumpfile format(2B) | dumpfile version (4B) | sync log version(4B)|
 * ---------------------------------------------------------------------
 * | sync log position (4B) | dumpfile size (8B)| data |
 * ------------------------------------------------------
 *
 * @param ht hash table
 */
int 
dumpfile(HashTable *ht)
{
    HashNode    *node;
    HashNode    **bks = ht->bunks;

    char        tmpfile[PATH_MAX];
    char        dumpfile[PATH_MAX];
    char        dumpfileok[PATH_MAX];
    char        dumpfiletime[PATH_MAX];
    int         i;
    long long   size = 0;
    struct timeval start, end;
    
    DINFO("dumpfile start ...\n");

    snprintf(dumpfile, PATH_MAX, "%s/%s", g_cf->datadir, DUMP_FILE_NAME);
    snprintf(tmpfile, PATH_MAX, "%s/%s.tmp", g_cf->datadir, DUMP_FILE_NAME);
    snprintf(dumpfileok, PATH_MAX, "%s/%s.ok", g_cf->datadir, DUMP_FILE_NAME);

    DINFO("dumpfile to tmp: %s\n", tmpfile);
   
    gettimeofday(&start, NULL);
    FILE    *fp = fopen64(tmpfile, "wb");

    unsigned short formatver = DUMP_FORMAT_VERSION;
    ffwrite(&formatver, sizeof(short), 1, fp);
    DINFO("write format version %d\n", formatver);

    g_runtime->dumpver += 1;
    unsigned int dumpver = g_runtime->dumpver;
    ffwrite(&dumpver, sizeof(int), 1, fp);
    DINFO("write dumpfile version %d\n", dumpver);

    ffwrite(&g_runtime->logver, sizeof(int), 1, fp);
    DINFO("write logfile version %d\n", g_runtime->logver);

    ffwrite(&g_runtime->synclog->index_pos, sizeof(int), 1, fp);
    DINFO("write logfile pos: %d\n", g_runtime->synclog->index_pos);
    ffwrite(&size, sizeof(long long), 1, fp);

    unsigned char keylen;
    int datalen;
    int n;
    int dump_count = 0;

    for (i = 0; i < HASHTABLE_BUNKNUM; i++) {
        node = bks[i];
        while (NULL != node) {
            DINFO("start dump key: %s\n", node->key);
            unsigned char wchar; 
            wchar = node->type;
            //ffwrite(&node->type, sizeof(char), 1, fp);
            ffwrite(&wchar, sizeof(char), 1, fp);
            wchar = node->sortfield;
            //ffwrite(&node->sortfield, sizeof(char), 1, fp);
            ffwrite(&wchar, sizeof(char), 1, fp);
            wchar = node->valuetype;
            //ffwrite(&node->valuetype, sizeof(char), 1, fp);
            ffwrite(&wchar, sizeof(char), 1, fp);
            keylen = strlen(node->key);
            ffwrite(&keylen, sizeof(char), 1, fp);
            //DINFO("dump keylen: %d\n", keylen);
            ffwrite(node->key, keylen, 1, fp);
            ffwrite(&node->valuesize, sizeof(char), 1, fp);
            ffwrite(&node->masksize, sizeof(char), 1, fp);
            wchar = node->masknum;
            //ffwrite(&node->masknum, sizeof(char), 1, fp);
            ffwrite(&wchar, sizeof(char), 1, fp);

            if (node->masknum > 0) {
                ffwrite(node->maskformat, sizeof(char) * node->masknum, 1, fp);
            }
            /*for (k = 0; k < node->masknum; k++) {
                DINFO("dump mask, k:%d, mask:%d\n", k, node->maskformat[k]);
            }*/
            ffwrite(&node->used, sizeof(int), 1, fp);
            datalen = node->valuesize + node->masksize;

            DataBlock *dbk = node->data;

            while (dbk) {
                char *itemdata = dbk->data;
                for (n = 0; n < dbk->data_count; n++) {
                    if (dataitem_have_data(node, itemdata, 0)) {    
                        ffwrite(itemdata, datalen, 1, fp);
                        dump_count += 1;
                    }
                    itemdata += datalen;
                }
                dbk = dbk->next;
            }
            node = node->next;
        }
    }
    
    DINFO("dump count: %d\n", dump_count);
    
    size = ftell(fp);

    fseek(fp,  DUMP_HEAD_LEN - sizeof(long long), SEEK_SET);
    ffwrite(&size, sizeof(long long), 1, fp);

    fclose(fp);
    
    //把dump.dat.tmp变成dump.dat.ok，以备后面使用
    int ret = rename(tmpfile, dumpfileok);
    DINFO("rename %s to %s return: %d\n", tmpfile, dumpfileok, ret);
    if (ret == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("dumpfile rename error: %s\n",  errbuf);
    }
    
    if (isfile(dumpfile)) {
        //保存当前的dump.dat，在后面加上时间以区分
        time_t timep;
        struct tm result, *p;

        time(&timep);
        memcpy(&g_runtime->last_dump, &timep, sizeof(time_t));
        localtime_r(&timep, &result);
        p = &result;
        snprintf(dumpfiletime, PATH_MAX, "%s/%s.%d%02d%02d.%02d%02d%02d", g_cf->datadir, DUMP_FILE_NAME, 
            p->tm_year + 1900, 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec); 
        //如果加了这个日期后缀的文件存在，再在文件后面加一个随机数
        int i = 1;
        while (isfile(dumpfiletime)) {
            snprintf(dumpfiletime, PATH_MAX, "%s/%s.%d%02d%02d.%02d%02d%02d.%02d", g_cf->datadir, DUMP_FILE_NAME,
                p->tm_year + 1900, 1 + p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec, i);
            i++;
        }

        //如果dump.dat存在，则在后面加上日期后缀
        ret = rename(dumpfile, dumpfiletime);
        DINFO("rename %s to %s return: %d\n", dumpfile, dumpfiletime, ret);
        if (ret == -1) {
            char errbuf[1024];
            strerror_r(errno, errbuf, 1024);
			DERROR("dumpfile rename error %s\n",  errbuf);
        }
    }    
    //把当前的dump.dat.ok，改名为dump.dat    
    ret = rename (dumpfileok, dumpfile);
    DINFO("rename %s to %s return: %d\n", dumpfileok, dumpfile, ret);
    if (ret == -1) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
		DERROR("dumpfile rename error %s\n",  errbuf);
    }
           
    gettimeofday(&end, NULL);
    DNOTE("dump time: %u us\n", timediff(&start, &end));
    return ret;
}

/**
 * @param ht
 * @param filename  dumpfile name
 * @param localdump  is local dump file. 
 */
int
dumpfile_load(HashTable *ht, char *filename, int localdump)
{
    FILE    *fp;
    int     filelen;
    int        ret;
    struct timeval start, end;

    DNOTE("dumpfile load: %s\n", filename);
    gettimeofday(&start, NULL);
    fp = fopen64(filename, "rb");
    if (NULL == fp) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("open dumpfile %s error: %s\n", filename,  errbuf);
        return -1;
    }
   
    fseek(fp, 0, SEEK_END);
    filelen = ftell(fp);
    DINFO("dumpfile len: %d\n", filelen);
    fseek(fp, 0, SEEK_SET);
    unsigned short dumpfver;
    ret = ffread(&dumpfver, sizeof(short), 1, fp);
    DINFO("load format ver: %d\n", dumpfver);

    if (dumpfver != DUMP_FORMAT_VERSION) {
        DERROR("dumpfile format version error: %d, %d\n", dumpfver, DUMP_FORMAT_VERSION);
        fclose(fp);
        return -2;
    }
    
    unsigned int dumpver;
    ret = ffread(&dumpver, sizeof(int), 1, fp);
    DINFO("load dumpfile ver: %u\n", dumpver);
    if (localdump) {
        g_runtime->dumpver = dumpver;
    }

    unsigned int dumplogver;
    ret = ffread(&dumplogver, sizeof(int), 1, fp);
    DINFO("load dumpfile log ver: %u\n", dumplogver);
    if (localdump) {
        g_runtime->dumplogver = dumplogver;
    }

    unsigned int dumplogpos;
    ret = ffread(&dumplogpos, sizeof(int), 1, fp);
    DINFO("load dumpfile log pos: %u\n", dumplogpos);
    if (localdump) {
        g_runtime->dumplogpos = dumplogpos;
    }


    long long size;
    ret = ffread(&size, sizeof(long long), 1, fp);

    unsigned char keylen;
    unsigned char masklen;
    unsigned char masknum;
    unsigned char maskformat[HASHTABLE_MASK_MAX_ITEM];
    unsigned char valuelen;
    unsigned char valuetype;
    unsigned int  itemnum;
    char          key[256];
    unsigned char type;
    unsigned char sortfield;
    int           i;
    int           datalen;
    int           load_count = 0;
    unsigned int  block_data_count_max = g_cf->block_data_count[g_cf->block_data_count_items - 1];

    while (ftell(fp) < filelen) {
        //DINFO("--- cur: %d, filelen: %d, %d ---\n", (int)ftell(fp), filelen, feof(fp));
        ret = ffread(&type, sizeof(char), 1, fp);
        ret = ffread(&sortfield, sizeof(char), 1, fp);
        ret = ffread(&valuetype, sizeof(char), 1, fp);
        ret = ffread(&keylen, sizeof(unsigned char), 1, fp);
        //DINFO("keylen: %d\n", keylen);
        ret = ffread(key, keylen, 1, fp);
        key[keylen] = 0;
        //DINFO("key: %s\n", key);
        
        ret = ffread(&valuelen, sizeof(unsigned char), 1, fp);
        //DINFO("valuelen: %d\n", valuelen);
        ret = ffread(&masklen, sizeof(unsigned char), 1, fp);
        //DINFO("masklen: %d\n", masklen);
        ret = ffread(&masknum, sizeof(char), 1, fp);
        //DINFO("masknum: %d\n", masknum);
        if (masknum > 0) {
            ret = ffread(maskformat, sizeof(char) * masknum, 1, fp);
        }
            
        /*for (i = 0; i < masknum; i++) {
            DINFO("maskformat, i:%d, mask:%d\n", i, maskformat[i]);
        }*/
        ret = ffread(&itemnum, sizeof(unsigned int), 1, fp);
        datalen = valuelen + masklen;
        //DINFO("itemnum: %d, datalen: %d\n", itemnum, datalen);

        unsigned int maskarray[HASHTABLE_MASK_MAX_ITEM] = {0};
        for (i = 0; i < masknum; i++) {
            maskarray[i] = maskformat[i];
        }
        //DINFO("create info, key:%s, valuelen:%d, masknum:%d\n", key, valuelen, masknum);
        ret = hashtable_key_create_mask(ht, key, valuelen, maskarray, masknum, type, valuetype);
        if (ret != MEMLINK_OK) {
            DERROR("hashtable_key_create_mask error, ret:%d\n", ret);
            return -2;
        }
        HashNode    *node = hashtable_find(ht, key);
        DataBlock   *dbk  = NULL;
        DataBlock   *newdbk;
        char        *itemdata = NULL;

        for (i = 0; i < itemnum; i++) {
            //DINFO("i: %d\n", i);
            if (i % block_data_count_max == 0) {
                //DINFO("create new datablock ...\n");
                /*
                if (itemnum == 1) {
                    newdbk = mempool_get(g_runtime->mpool, sizeof(DataBlock) + datalen); 
                    newdbk->data_count = 1;
                }else{
                    newdbk = mempool_get(g_runtime->mpool, sizeof(DataBlock) + g_cf->block_data_count *datalen); 
                    newdbk->data_count = g_cf->block_data_count;
                }*/
                
                if (itemnum - i > block_data_count_max) {
                    newdbk = mempool_get2(g_runtime->mpool, block_data_count_max, datalen); 
                }else{
                    newdbk = mempool_get2(g_runtime->mpool, itemnum-i, datalen); 
                }

                if (NULL == newdbk) {
                    DERROR("mempool_get NULL!\n");
                    MEMLINK_EXIT;
                }

                if (dbk == NULL) {
                    node->data = newdbk;
                }else{
                    dbk->next = newdbk;
                }
                newdbk->prev = dbk;
                dbk = newdbk;
                //node->all += newdbk->data_count;

                itemdata = dbk->data;
            }
            ret = ffread(itemdata, datalen, 1, fp);
            ret = dataitem_check_data(node, itemdata);
            if (ret == MEMLINK_VALUE_VISIBLE) {
                dbk->visible_count++;
            }else{
                dbk->tagdel_count++;
            }
            node->used++;

            /*char buf[256] = {0};
            memcpy(buf, itemdata, node->valuesize);
            DINFO("load value: %s\n", buf);*/
            
            itemdata += datalen;
            load_count += 1;
        }
        node->data_tail = dbk;
    }
    fclose(fp);
    //DINFO("load count: %d\n", load_count);

    gettimeofday(&end, NULL);
    DNOTE("load dump %d time: %u us\n", load_count, timediff(&start, &end));

    return 0;
}

int
dumpfile_logver(char *filename, unsigned int *logver, unsigned int *logpos)
{
    int  ret;
    FILE    *dumpf;

    //dumpf = fopen(filename, "r");
    dumpf = fopen64(filename, "r");
    if (dumpf == NULL) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("open file %s error! %s\n", filename,  errbuf);
        return -1;
    }

    int pos = sizeof(short) + sizeof(int);
    fseek(dumpf, pos, SEEK_SET);

    ret = fread(&logver, sizeof(int), 1, dumpf);
    if (ret != sizeof(int)) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("fread error: %s\n",  errbuf);
        fclose(dumpf);
        return -1;
    }
    ret = fread(&logpos, sizeof(int), 1, dumpf);
    if (ret != sizeof(int)) {
        char errbuf[1024];
        strerror_r(errno, errbuf, 1024);
        DERROR("fread error: %s\n",  errbuf);
        fclose(dumpf);
        return -1;
    }

    fclose(dumpf);

    return 0;
}

void
dumpfile_call_loop(int fd, short event, void *arg)
{
    dumpfile_call();
    
    struct timeval    tv;
    struct event *timeout = arg;

    evutil_timerclear(&tv);
    tv.tv_sec = g_cf->dump_interval * 60; 
    event_add(timeout, &tv);
}

int
dumpfile_call()
{
    int ret;

    pthread_mutex_lock(&g_runtime->mutex);
    //g_runtime->indump = 1;
    ret = dumpfile(g_runtime->ht);
    //g_runtime->indump = 0;
    pthread_mutex_unlock(&g_runtime->mutex);
    
    return ret;
}

/**
 * @}
 */
