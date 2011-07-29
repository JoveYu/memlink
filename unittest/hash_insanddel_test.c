#include "hashtest.h"

int main()
{
#ifdef DEBUG
	logfile_create("stdout", 3);
#endif
	HashTable* ht;
	char key[64];
	int valuesize = 8;
	char val[64];
	unsigned int attrformat[3]   = {4, 3, 1};
	unsigned int attrarray[3][3] = { {8, 3, 1}, { 7, 2,1}, {6, 2, 1} }; 
	int num  = 200;
	int attrnum = 3;
	int ret;
	int i = 0;

	char *conffile;
	conffile = "memlink.conf";
	DINFO("config file: %s\n", conffile);
	myconfig_create(conffile);

	my_runtime_create_common("memlink");
	ht = g_runtime->ht;

	///////////begin test;
	//test1 : hashtable_add_info_attr - create key
	for (i = 0; i < num; i++) {
		sprintf(key, "heihei%03d", i);
		hashtable_key_create_attr(ht, key, valuesize, attrformat, attrnum, 
                    MEMLINK_LIST, MEMLINK_VALUE_STRING);
	}
	for (i = 0; i < num; i++) {
		sprintf(key, "heihei%03d", i);
		HashNode* pNode = hashtable_find(ht, key);
		if (NULL == pNode) {
			DERROR("hashtable_add_info_attr error. can not find %s\n", key);
			return -1;
		}
	}

    //test : hashtable_add_attr insert num value
	DINFO("1 insert 1000 \n");

	int pos = 0;
	HashNode *node = NULL;
	DataBlock *dbk = NULL;
	char	 *item = NULL; 
	num = 200;

	for (i = 0; i < num; i++) {
		sprintf(val, "value%03d", i);
		pos = i;
		ret = hashtable_add_attr(ht, key, val, attrarray[i%3], 3, pos);
		if (ret < 0) {
			DERROR("hashtable_add_attr err! ret:%d, val:%s, pos:%d \n", ret, val, pos);
			return ret;
		}
	}

	DINFO("2 insert %d\n", num);
    
    MemLinkStat stat;
    ret = hashtable_stat(ht, key, &stat);
    if (ret != MEMLINK_OK) {
        DERROR("stat error!\n");
    }

    DINFO("blocks:%d, data_used:%d\n", stat.blocks, stat.data_used);

	for (i = 0; i < num; i++) {
		sprintf(val, "value%03d", i);
		ret = hashtable_find_value(ht, key, val, &node, &dbk, &item);
		if (ret < 0) {
			DERROR("not found value: %d, %s\n", ret, val);
			return ret;
		}
	}
	
	//hashtable_print(g_runtime->ht, key);
	sprintf(val, "value%03d", 50);
	ret = hashtable_del(ht, key, val);
	if (ret < 0) {
		DERROR("del value: %d, %s\n", ret, val);
		return ret;
	}
    DINFO("deleted %s\n", val);	
	hashtable_print(g_runtime->ht, key);

    ret = hashtable_find_value(ht, key, val, &node, &dbk, &item);
    if (ret >= 0) {
        DERROR("found value: %d, %s\n", ret, val);
        return ret;
    }

	ret = hashtable_add_attr(ht, key, val, attrarray[2], attrnum, 100);
	if (ret < 0) {
		DERROR("add value err: %d, %s\n", ret, val);
		return ret;
	}

	ret = hashtable_find_value(ht, key, val, &node, &dbk, &item);
	if (ret < 0) {
		DERROR("not found value: %d, %s\n", ret, val);
		return ret;
	}
	DINFO("test: insert ok!\n");

    hashtable_print(ht, key);
    /////////// hashtable_del
	for (i = 0; i < num; i++) {
		sprintf(val, "value%03d", i);
		//pos = i;
		ret = hashtable_del(ht, key, val);
		if (ret < 0) {
			DERROR("hashtable_del err! ret:%d, val:%s \n", ret, val);
			return ret;
		}
	}
	DINFO("del %d!\n", num);

	for (i = 0; i < num; i++) {
		sprintf(val, "value%03d", i);
		ret = hashtable_find_value(ht, key, val, &node, &dbk, &item);
		if (ret >= 0) {
			DERROR("err should not found value: %d, %s\n", ret, key);
			return ret;
		}
	}
	
	DINFO("test: del ok!\n");
	return 0;
}
