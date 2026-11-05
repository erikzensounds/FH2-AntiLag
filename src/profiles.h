/* GPL-3.0. Loaded once on the initializer thread, never from a vehicle tick. */
#include <limits.h>
typedef struct {int decay,retention,cutoff,pop;} ALSSettings;
typedef struct {int model;ALSSettings settings;} ALSProfile;
typedef struct {int part,model;} ALSModelPart;
static ALSProfile profiles[256];
static unsigned profiles_count;
static ALSModelPart model_parts[65536];
static unsigned model_parts_count;
static ALSSettings base_settings;
static SRWLOCK profile_lock=SRWLOCK_INIT;
static volatile LONG profile_revision;
static wchar_t profile_path[32768];
static int selected_model=-1;
static int positive_decimal(const wchar_t *text,int *out){
    unsigned value=0;if(!*text)return 0;
    for(;*text;text++){
        if(*text<L'0'||*text>L'9'||value>((unsigned)INT_MAX-(unsigned)(*text-L'0'))/10)return 0;
        value=value*10+(unsigned)(*text-L'0');
    }
    *out=(int)value;return 1;
}
static int profile_value(const wchar_t *path,const wchar_t *section,const wchar_t *key,int fallback,int low,int high,int *out){
    wchar_t text[64];DWORD n=GetPrivateProfileStringW(section,key,L"",text,64,path);
    if(!n){*out=fallback;return 1;}
    return n<63 && positive_decimal(text,out) && *out>=low && *out<=high;
}
static int read_profile(const wchar_t *path,const wchar_t *section,ALSSettings *out){
    wchar_t entries[4096];DWORD length=GetPrivateProfileSectionW(section,entries,4096,path);
    if(length>=4094)return 0;
    for(wchar_t *entry=entries;*entry;entry+=wcslen(entry)+1){
        wchar_t *equal=wcschr(entry,L'=');if(!equal)return 0;
        wchar_t saved=*equal;*equal=0;
        int known=!_wcsicmp(entry,L"PressureDecayMs")||!_wcsicmp(entry,L"RetentionPercent")||!_wcsicmp(entry,L"CutoffRPM")||!_wcsicmp(entry,L"PopIntervalMs");
        *equal=saved;if(!known||!equal[1])return 0;
    }
    ALSSettings candidate;
    if(!profile_value(path,section,L"PressureDecayMs",base_settings.decay,1000,8000,&candidate.decay) ||
       !profile_value(path,section,L"RetentionPercent",base_settings.retention,25,95,&candidate.retention) ||
       !profile_value(path,section,L"CutoffRPM",base_settings.cutoff,1000,6000,&candidate.cutoff) ||
       !profile_value(path,section,L"PopIntervalMs",base_settings.pop,100,500,&candidate.pop))return 0;
    *out=candidate;return 1;
}
static void profiles_load(const wchar_t *directory,ALSSettings defaults){
    base_settings=defaults;profiles_count=0;
    wchar_t path[32768];wcscpy(path,directory);wcscat(path,L"\\EO_AntiLag_Profiles.ini");
    wcscpy(profile_path,path);
    wchar_t *sections=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,65536*sizeof(wchar_t));
    if(!sections){log_line("ANTI-LAG PROFILES: allocation failed; global settings retained.\r\n");return;}
    DWORD n=GetPrivateProfileSectionNamesW(sections,65536,path);
    if(n>=65534){HeapFree(GetProcessHeap(),0,sections);log_line("ANTI-LAG PROFILES: section list too long; global settings retained.\r\n");return;}
    for(wchar_t *section=sections;*section;section+=wcslen(section)+1){
        int model=0;ALSSettings settings;
        if(_wcsnicmp(section,L"Car:",4) || !positive_decimal(section+4,&model) || model<=0){log_line("ANTI-LAG PROFILES: unrecognized section ignored; expected Car:<model ID>.\r\n");continue;}
        int duplicate=0;for(unsigned i=0;i<profiles_count;i++)if(profiles[i].model==model)duplicate=1;
        if(duplicate || !read_profile(path,section,&settings)){
            char line[160];snprintf(line,sizeof line,"ANTI-LAG PROFILES: invalid/duplicate profile for model %d ignored.\r\n",model);log_line(line);continue;
        }
        if(profiles_count==256){log_line("ANTI-LAG PROFILES: maximum 256 profiles; remaining sections ignored.\r\n");break;}
        profiles[profiles_count++]=(ALSProfile){model,settings};
    }
    HeapFree(GetProcessHeap(),0,sections);
    char line[128];snprintf(line,sizeof line,"ANTI-LAG PROFILES: loaded %u model profiles; missing values inherit global INI.\r\n",profiles_count);log_line(line);
}
static int profiles_map_models(const FH2PluginDatabaseFrame *f){
    model_parts_count=0;selected_model=-1;void *stmt=NULL;
    if(f->prepare(f->database,"SELECT Id,Ordinal FROM List_UpgradeEngine ORDER BY Id",-1,&stmt,NULL)||!stmt){if(stmt)f->finalize(stmt);return 0;}
    int result,valid=1,previous=-1;
    while((result=f->step(stmt))==100){
        int64_t part=f->column_int64(stmt,0),model=f->column_int64(stmt,1);
        if(part<0||part>INT_MAX||model<=0||model>INT_MAX||part<=previous||model_parts_count>=65536){valid=0;break;}
        model_parts[model_parts_count++]=(ALSModelPart){(int)part,(int)model};previous=(int)part;
    }
    int ended=f->finalize(stmt);
    if(!valid||result!=101||ended){model_parts_count=0;return 0;}
    return 1;
}
static int profile_model(int installed_engine_part){
    unsigned lo=0,hi=model_parts_count;
    while(lo<hi){unsigned mid=lo+(hi-lo)/2;
        if(model_parts[mid].part<installed_engine_part)lo=mid+1;else hi=mid;}
    return lo<model_parts_count && model_parts[lo].part==installed_engine_part?model_parts[lo].model:0;
}
static ALSSettings profiles_choose(int model,int *custom){
    AcquireSRWLockShared(&profile_lock);
    *custom=0;
    for(unsigned i=0;i<profiles_count;i++)if(model>0 && profiles[i].model==model){*custom=1;ALSSettings value=profiles[i].settings;ReleaseSRWLockShared(&profile_lock);return value;}
    ReleaseSRWLockShared(&profile_lock);
    return base_settings;
}
static int profiles_save(int model,const ALSSettings *settings){
    if(model<=0||!profile_path[0])return 0;
    if(settings && (settings->decay<1000||settings->decay>8000||settings->retention<25||settings->retention>95||settings->cutoff<1000||settings->cutoff>6000||settings->pop<100||settings->pop>500))return 0;
    wchar_t section[32],data[256];swprintf(section,32,L"Car:%d",model);
    if(settings){
        int n=swprintf(data,256,L"PressureDecayMs=%d|RetentionPercent=%d|CutoffRPM=%d|PopIntervalMs=%d|",settings->decay,settings->retention,settings->cutoff,settings->pop);
        if(n<0||n>=255)return 0;
        for(int i=0;i<n;i++)if(data[i]==L'|')data[i]=0;
    }
    AcquireSRWLockExclusive(&profile_lock);
    unsigned at=0;while(at<profiles_count && profiles[at].model!=model)at++;
    if(settings && at==profiles_count && profiles_count==256){ReleaseSRWLockExclusive(&profile_lock);return 0;}
    int ok=WritePrivateProfileSectionW(section,settings?data:NULL,profile_path)!=0;
    if(ok){
        if(settings){profiles[at]=(ALSProfile){model,*settings};if(at==profiles_count)profiles_count++;}
        else if(at<profiles_count){memmove(profiles+at,profiles+at+1,(profiles_count-at-1)*sizeof(*profiles));profiles_count--;}
        InterlockedIncrement(&profile_revision);
    }
    ReleaseSRWLockExclusive(&profile_lock);return ok;
}
