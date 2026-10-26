/* Whole original character pool/getter comparisons, compiled for each ABI. */
extern bg_character_t alliedClassCharacters[NUM_PLAYER_CLASSES], axisClassCharacters[NUM_PLAYER_CLASSES];
extern bg_character_t bg_characterPool[MAX_CHARACTERS];
extern qboolean bg_characterPoolInuse[MAX_CHARACTERS];
static int checkCharacterPool(unsigned char *b, int client) {
    int clearOff=client?0x3520:0x2ae60, findOff=client?0x35e0:0x2af20;
    int allocOff=client?0x3540:0x2ae80, getOff=client?0x34f0:0x2ae30;
    unsigned char *pool=b+(client?0x4c35e00:0x24544e0);
    unsigned char *axis=b+(client?0x4c2ea40:0x24540e0);
    unsigned char *allies=b+(client?0x4c32400:0x24542c0);
    int *used=(int *)(b+(client?0x4c35dc0:0x24544a0));
    int seed,i,step,mode,n=0; char text[96]; void *r; bg_character_t *s;
    if(sizeof(bg_character_t)!=(client?0x840:0x44)) {printf("character ABI %u\n",(unsigned)sizeof(bg_character_t));return -1;}
    for(seed=-1;seed<5;++seed)for(i=0;i<7;++i){
        r=((void *(__cdecl *)(int,int))(b+getOff))(seed,i);s=BG_GetCharacter(seed,i);
        if(r!=(seed==2?allies:axis)+i*sizeof(*s) || s!=(seed==2?alliedClassCharacters:axisClassCharacters)+i)return -2;++n;
    }
    for(seed=0;seed<64;++seed){
        memset(pool,0xa7,sizeof(bg_characterPool));memset(bg_characterPool,0xa7,sizeof(bg_characterPool));
        memset(used,0x13,sizeof(bg_characterPoolInuse));memset(bg_characterPoolInuse,0x13,sizeof(bg_characterPoolInuse));
        ((void(__cdecl *)(void))(b+clearOff))();BG_ClearCharacterPool();
        if(memcmp(pool,bg_characterPool,sizeof(bg_characterPool))||memcmp(used,bg_characterPoolInuse,sizeof(bg_characterPoolInuse)))return -3;
        for(i=0;i<16;++i)if((seed>>(i%6))&1){used[i]=bg_characterPoolInuse[i]=1;sprintf(text,"character_%d",i);strcpy((char *)pool+i*sizeof(*s),text);strcpy(bg_characterPool[i].characterFile,text);}
        for(step=0;step<40;++step)for(mode=0;mode<2;++mode){
            if(step==39){memset(text,'x',95);text[95]=0;}else if(step==38)text[0]=0;else sprintf(text,step&1?"CHARACTER_%d":"character_%d",step);
            r=((void *(__cdecl *)(const char *))(b+(mode?allocOff:findOff)))(text);
            s=mode?BG_FindFreeCharacter(text):BG_FindCharacter(text);
            if((!r)!=(!s)||(s&&(char *)r-(char *)pool!=(char *)s-(char *)bg_characterPool))return -4;
            if(memcmp(pool,bg_characterPool,sizeof(bg_characterPool))||memcmp(used,bg_characterPoolInuse,sizeof(bg_characterPoolInuse)))return -5;++n;
        }
    }
    return n;
}
