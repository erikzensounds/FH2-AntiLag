/* GPL-3.0. Catalogue is applied only to Extra Options' merged memory database. */
#include "catalog_sql.h"
typedef struct {int part,id;} ALSPart;
static ALSPart catalog_parts[4096];
static unsigned catalog_count;
static volatile LONG catalog_ready;
static int catalog_query(const FH2PluginDatabaseFrame *f,const char *sql,int check,int lookup){
    void *stmt=NULL;
    if(f->prepare(f->database,sql,-1,&stmt,NULL)||!stmt){if(stmt)f->finalize(stmt);return 0;}
    int result,valid=1,rows=0;
    while((result=f->step(stmt))==100){
        rows++;
        if(check && f->column_int64(stmt,0)!=1)valid=0;
        if(lookup){
            if(catalog_count>=4096){valid=0;break;}
            catalog_parts[catalog_count++]=(ALSPart){(int)f->column_int64(stmt,0),(int)f->column_int64(stmt,1)};
        }
    }
    int ended=f->finalize(stmt);
    return valid && result==101 && !ended && (!check || rows==1);
}
__declspec(dllexport) BOOL WINAPI FH2PluginDatabaseUpdate(const FH2PluginDatabaseFrame *f){
    InterlockedExchange(&catalog_ready,0);catalog_count=0;
    model_parts_count=0;selected_model=-1;
    if(!f || f->size<sizeof(*f) || f->api_version!=1 || !f->database || !f->prepare || !f->step || !f->finalize || !f->column_int64)return FALSE;
    for(unsigned i=0;i<sizeof(catalog_sql)/sizeof(*catalog_sql);i++){
        if(!catalog_query(f,catalog_sql[i].sql,catalog_sql[i].check,catalog_sql[i].lookup)){
            catalog_query(f,"ROLLBACK TO fh2_antilag_catalog",0,0);
            catalog_query(f,"RELEASE fh2_antilag_catalog",0,0);
            catalog_count=0;log_line("ANTI-LAG SHOP: catalog refused; transaction rolled back; anti-lag disabled.\r\n");return FALSE;
        }
    }
    if(!profiles_map_models(f))log_line("ANTI-LAG PROFILES: model mapping unavailable; global settings will be used.\r\n");
    InterlockedExchange(&catalog_ready,1);
    char line[160];snprintf(line,sizeof line,"ANTI-LAG SHOP: %u extra race turbo parts added in memory; original parts preserved.\r\n",catalog_count);log_line(line);
    return TRUE;
}
static int catalog_equipped(void *car){
    if(!InterlockedCompareExchange(&catalog_ready,0,0))return 0;
    // Candidate layout: contiguous installed IDs at car+128; corroborated by
    // 35 native table matches and RX7 twin turbo row 178003 in a read-only session.
    // Needs purchase/reload validation before being considered stable.
    for(unsigned i=0;i<catalog_count;i++){
        int part=catalog_parts[i].part;
        if(integer(car,0x128+4*(unsigned)part)==catalog_parts[i].id)return 1;
    }
    return 0;
}
