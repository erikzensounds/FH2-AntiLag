/* GPL-3.0. Native Tune Car integration, adapted from Extra Options UI patterns.
   Prototype: properties and resources fail closed; no game save writes. */
#include "turbo_xaml.h"
#include "turbo_signatures.h"
typedef struct {void **vtable;unsigned char header[0x20];void *getter,*setter,*unused;const char *name;void *type;uint64_t flags;} TurboProperty;
typedef struct {TurboProperty *property;void *next;} TurboLink;
typedef struct {void **vtable;unsigned char header[0x20];char *bytes;int length,capacity;} TurboResource;
_Static_assert(sizeof(TurboProperty)==0x58,"native property size");
_Static_assert(offsetof(TurboResource,bytes)==0x28,"native resource layout");
static TurboProperty turbo_properties[7];static TurboLink turbo_links[7];
static void *turbo_modes,*turbo_page,*turbo_captured;
static volatile LONG turbo_init_thread;
static void *turbo_initializing_page;
static int turbo_appended,turbo_focus_pending;
static void (*turbo_defaults_next)(void *);
static unsigned char (*turbo_advance_next)(void *);
static void (*turbo_dependency_next)(void *,void *,void *);
static volatile LONG turbo_ready,turbo_xaml_ready,turbo_resource_seen,turbo_init_seen,turbo_legacy_seen;
static void (*turbo_init_next)(void *),(*turbo_change_next)(void *),(*turbo_update_next)(void *);
static void **(*turbo_factory_next)(void **),*(*turbo_resource_next)(void *,void **);
static void *turbo_resource_vtable[7];
int entry_hook_chain_thunk(unsigned char *,size_t,unsigned,void *,void **);
static void turbo_release(void *object){if(object&&InterlockedDecrement((volatile LONG *)((char *)object+0x10))==0)((void (*)(void *))(image+0x257730))(object);}
static int turbo_context(void *page,int *model){
    *model=0;if(!page||!readable(page,0x4d8))return 0;
    uintptr_t params=(uintptr_t)*(void **)((char *)page+0x4d0);
    if(params<0x1200)return 0;
    void *car=(void *)(params-0x1200);
    if(!readable(car,0x4c10)||*(void **)car!=image+0x2c5cb80)return 0;
    *model=profile_model(integer(car,0x128));
    return *model>0&&catalog_equipped(car);
}
static int turbo_property_index(TurboProperty *p){
    uintptr_t at=(uintptr_t)p,first=(uintptr_t)turbo_properties;
    return at>=first&&at<first+sizeof(turbo_properties)&&(at-first)%sizeof(*p)==0?(int)((at-first)/sizeof(*p)):-1;
}
static void turbo_refresh(void *page){
#ifdef ALS_UI_TEST
    (void)page;
#else
    for(unsigned i=0;i<7;i++)((void (*)(void *,void *))(image+0xcf9c60))(page,&turbo_properties[i]);
#endif
}
static int turbo_category_route(int owned,int selected){return !owned?0:selected>=0&&selected<9?0:selected==9?1:-1;}
static void **turbo_get(void **out,TurboProperty *property,void *page){
    *out=NULL;int which=turbo_property_index(property),model=0,custom=0;
    int editable=turbo_context(page,&model);ALSSettings s=profiles_choose(model,&custom);
    if(which==5){*out=turbo_modes;if(*out)InterlockedIncrement((volatile LONG *)((char *)*out+0x10));return out;}
    if(which==4||which==6){int value=which==4?custom:editable;return ((void **(*)(void **,const int *))(image+0x4702d0))(out,&value);}
    float value=which==0?(float)s.decay:which==1?(float)s.retention:which==2?(float)s.cutoff:(float)s.pop;
    return ((void **(*)(void **,const float *))(image+0x3f5990))(out,&value);
}
static void turbo_set(TurboProperty *property,void *page,void *boxed){
    int which=turbo_property_index(property),model=0,custom=0;
    if(which<0||which>3||!boxed||!readable(boxed,0x2c)||!turbo_context(page,&model))return;
    ALSSettings s=profiles_choose(model,&custom);int ok=0;
    {
        float value=number(boxed,0x28);const int lows[]={1000,25,1000,100},highs[]={8000,95,6000,500},steps[]={100,1,100,10};
        if(!(value>=lows[which]&&value<=highs[which]))return;
        int adjusted=lows[which]+(int)((value-lows[which])/steps[which]+.5f)*steps[which];
        int *field=which==0?&s.decay:which==1?&s.retention:which==2?&s.cutoff:&s.pop;
        if(*field==adjusted)return;*field=adjusted;ok=profiles_save(model,&s);
    }
    log_line(ok?"TURBO MENU: car profile saved; applies on the next driving update.\r\n":"TURBO MENU: profile write failed; previous settings retained.\r\n");
    turbo_refresh(page);
}
static int turbo_reset_profile(void *page){
    int model=0;if(!turbo_context(page,&model))return 0;
    ALSSettings defaults={6000,85,1800,140};
    int ok=profiles_save(model,&defaults);if(ok)turbo_refresh(page);
    log_line(ok?"TURBO MENU: current car reset to factory anti-lag defaults.\r\n":"TURBO MENU: default reset could not save car profile.\r\n");return ok;
}
static void turbo_defaults(void *parameters){
    turbo_defaults_next(parameters);
    if(turbo_ready&&turbo_page&&parameters==(char *)turbo_page+0x4c8)turbo_reset_profile(turbo_page);
}
static int turbo_make_modes(void){
    void *type=((void *(*)(void *))(image+0x1476110))(NULL);
    void *items=((void *(*)(size_t,void *))(image+0x14424e0))(0x58,type);if(!items)return 0;
    ((void (*)(void *))(image+0xaefd60))(items);
    const unsigned vtables[]={0x2db73f8,0x2db7470,0x2db74d0,0x2db74e8,0x2db7508},offsets[]={0,0x30,0x38,0x40,0x48};
    for(unsigned i=0;i<5;i++)*(void **)((char *)items+offsets[i])=image+vtables[i];
    const wchar_t *labels[]={L"Global",L"This car"};
    for(int i=0;i<2;i++){void *text=NULL;((void **(*)(void **,const wchar_t *))(image+0x13fe8e0))(&text,labels[i]);if(!text){turbo_release(items);return 0;}
        ((void (*)(void *,int,void *))(image+0xb52170))(items,i,text);turbo_release(text);}
    turbo_modes=items;return 1;
}
static int turbo_register(void){
    ((void (*)(void))(image+0xe98cd0))();
    const unsigned factories[]={0xed6e90,0xed6e90,0xed6e90,0xed6e90,0xed81f0,0xed77d0,0xed81f0};
    const char *names[]={"TurboDecay","TurboRetention","TurboCutoff","TurboPop","TurboMode","TurboModes","TurboEditable"};
    for(unsigned i=0;i<7;i++){
        TurboProperty *t=((TurboProperty *(*)(void))(image+factories[i]))();
        const char *expected=i<4?"Gamma":i==5?"OnOffOptions":"Subtitles";
        if(!t||!readable(t,sizeof(*t))||!t->name||strcmp(t->name,expected))return 0;
        TurboProperty *p=&turbo_properties[i];((void (*)(void *))(image+0x143eb80))(p);
        p->vtable=t->vtable;memcpy((char *)p+0x28,(char *)t+0x28,0x30);p->name=names[i];p->getter=turbo_get;p->setter=i<5?turbo_set:NULL;
        turbo_links[i]=(TurboLink){p,NULL};
    }
    if(!turbo_make_modes())return 0;
    for(unsigned i=0;i<7;i++)((void (*)(void *,void *))(image+0x155ee30))(image+0x3541a50,&turbo_links[i]);
    return 1;
}
static void **turbo_factory(void **out){
    void **result=turbo_factory_next(out);
    if((DWORD)InterlockedCompareExchange(&turbo_init_thread,0,0)==GetCurrentThreadId()&&!turbo_captured&&out&&*out&&readable(*out,0x58)&&*(void **)*out==image+0x2e59100){
        turbo_captured=*out;InterlockedIncrement((volatile LONG *)((char *)*out+0x10));
    }
    return result;
}
static int turbo_append_category(void *page,void *items){
    if(!items)return 0;
    void *data=*(void **)((char *)items+0x50);
    int count=readable(data,0x34)?integer(data,0x30):-1;
    if(count!=9){log_line("TURBO MENU: unexpected category count; Turbo tab not appended.\r\n");return 0;}
    void *label=NULL,*item=NULL;
    ((void **(*)(void **,const wchar_t *))(image+0x13fe8e0))(&label,L"TURBO");
    if(label)((void **(*)(void **,void **))(image+0xe741c0))(&item,&label);
    if(item){
        turbo_page=page;
        ((void (*)(void *,int,void *))(*(void ***)items)[0x40/8])(items,9,item);
    }
    int ok=item!=NULL;turbo_release(item);turbo_release(label);return ok;
}
static int turbo_should_append(int ready,int same_thread,int matching_selector,int matching_key,int captured,int appended){
    return ready&&same_thread&&matching_selector&&matching_key&&captured&&!appended;
}
static void turbo_dependency(void *object,void *key,void *value){
    /* Populate before ItemsSource assignment: ItemsSelector builds its navigation
       from the collection at attachment, not from a later rendered addition. */
    if(turbo_ready&&turbo_initializing_page&&
       (DWORD)InterlockedCompareExchange(&turbo_init_thread,0,0)==GetCurrentThreadId()){
        void *selector=*(void **)((char *)turbo_initializing_page+0x480);
        if(turbo_should_append(1,1,object==selector,key==*(void **)(image+0x3696d08),turbo_captured!=NULL,turbo_appended)){
            turbo_appended=turbo_append_category(turbo_initializing_page,turbo_captured);
            log_line(turbo_appended?"TURBO MENU: tenth category inserted BEFORE ItemsSource navigation attachment.\r\n":"TURBO MENU: early category insertion failed; native categories retained.\r\n");
        }
    }
    turbo_dependency_next(object,key,value);
}
static void turbo_init(void *page){
    if(!InterlockedExchange(&turbo_init_seen,1))log_line(turbo_xaml_ready?"TURBO MENU: native Tune Car initialization reached; extended page ready.\r\n":"TURBO MENU: native Tune Car initialization reached BEFORE extended XAML; category withheld.\r\n");
    if(!turbo_ready||!turbo_xaml_ready||InterlockedCompareExchange(&turbo_init_thread,(LONG)GetCurrentThreadId(),0)){turbo_init_next(page);return;}
    turbo_captured=NULL;turbo_page=NULL;turbo_initializing_page=page;turbo_appended=0;turbo_focus_pending=0;
    turbo_init_next(page);
    void *items=turbo_captured;turbo_captured=NULL;turbo_initializing_page=NULL;
    InterlockedExchange(&turbo_init_thread,0);
    if(turbo_appended)turbo_refresh(page);
    else log_line("TURBO MENU: early ItemsSource attachment not captured; category withheld.\r\n");
    turbo_release(items);
}
static int turbo_current_index(void *page){
#ifdef ALS_UI_TEST
    extern int turbo_test_index;(void)page;return turbo_test_index;
#else
    void *selector=readable(page,0x488)?*(void **)((char *)page+0x480):NULL,*boxed=NULL;
    if(!selector)return -1;
    ((void **(*)(void *,void **,void *))(image+0x15e0f10))(selector,&boxed,*(void **)(image+0x36d1dd8));
    int selected=readable(boxed,0x2c)?integer(boxed,0x28):-1;turbo_release(boxed);
    return selected;
#endif
}
static int turbo_focus_panel(void *page);
static void turbo_update(void *page){
    if(page==turbo_page&&turbo_ready){int index=turbo_current_index(page);if(index<0||index>=9)*((unsigned char *)page+0x479)=1;}
    turbo_update_next(page);
    if(page==turbo_page&&turbo_ready&&turbo_focus_pending){
        if(turbo_current_index(page)!=9)turbo_focus_pending=0;
        else if(turbo_focus_panel(page)){
            turbo_focus_pending=0;log_line("TURBO MENU: deferred control focus acquired.\r\n");
        }else if(!--turbo_focus_pending)log_line("TURBO MENU: deferred focus timed out.\r\n");
    }
}
static int turbo_show_panel(void *page){
#ifdef ALS_UI_TEST
    extern int turbo_test_show(void *,const char *,const char *,int);
    return turbo_test_show(page,"LayoutRoot","VisualState_Turbo",1);
#else
    return ((unsigned char (*)(void *,const char *,const char *,int))(image+0x1410a70))(page,"LayoutRoot","VisualState_Turbo",1);
#endif
}
static int turbo_focus_named(void *page,const char *control){
#ifdef ALS_UI_TEST
    extern int turbo_test_focus(void *,const wchar_t *);(void)control;return turbo_test_focus(page,L"ALS_TurboPanel");
#else
    /* Name lookup takes a native ref-counted string, not a boxed UI value. */
    __declspec(align(16)) unsigned char text[0x60]={0};
    ((void *(*)(void *,const char *,int))(image+0x1663220))(text,control,-1);
    void *name=text;InterlockedIncrement((volatile LONG *)(text+0x10));
    void *panel=((void *(*)(void **,void *))(image+0xc63120))(&name,page);
    ((void (*)(void *))(image+0x16650a0))(text);
    return panel?((unsigned char (*)(void *))(image+0x141a390))(panel):0;
#endif
}
static int turbo_focus_panel(void *page){
    return turbo_focus_named(page,"ALS_DecaySlider")||turbo_focus_named(page,"ALS_TurboPanel");
}
static void turbo_select_category(void *page,int index){
#ifdef ALS_UI_TEST
    extern void turbo_test_restore(int);(void)page;turbo_test_restore(index);
#else
    void *selector=*(void **)((char *)page+0x480),*boxed=NULL;
    ((void **(*)(void **,const int *))(image+0x4702d0))(&boxed,&index);
    if(selector&&boxed)((void (*)(void *,void *,void *))(image+0x15f8a70))(selector,*(void **)(image+0x36d1dd8),boxed);
    turbo_release(boxed);
#endif
}
static unsigned char turbo_advance(void *page){
    if(page==turbo_page&&turbo_ready){
        int selected=turbo_current_index(page);
        if(selected==8){
            log_line("TURBO MENU: advancing from native Differential boundary to TURBO.\r\n");
            turbo_select_category(page,9);return 1;
        }
        if(selected==9)return 1; /* Consume tab-next at the final tab. */
    }
    return turbo_advance_next(page);
}
static void turbo_change(void *page){
    if(page!=turbo_page||!turbo_ready){turbo_change_next(page);return;}
    int selected=turbo_current_index(page);
    int route=turbo_category_route(1,selected);
    if(!route){turbo_change_next(page);return;}
    if(route!=1)return; // Never let the native nine-entry table read index 9+.
    if(!turbo_show_panel(page)){
        log_line("TURBO MENU: LayoutRoot rejected Turbo state; restoring native Differential selection.\r\n");
        turbo_select_category(page,8);return;
    }
    int model=0,editable=turbo_context(page,&model);char status[128];
    snprintf(status,sizeof(status),"TURBO MENU: tuned model=%d special turbo editable=%d.\r\n",model,editable);log_line(status);
    int focused=turbo_focus_panel(page);turbo_focus_pending=focused?0:120;
    log_line(focused?"TURBO MENU: TURBO state active and panel focused.\r\n":"TURBO MENU: TURBO state active, but panel focus failed.\r\n");
    // Native update also indexes the nine-entry table when this flag is zero.
    *((unsigned char *)page+0x479)=1;turbo_refresh(page);
}
static void *turbo_delete_resource(TurboResource *resource,unsigned flags){
    ((void (*)(void *))(image+0x1442050))(resource);
    if(flags&1)((void (*)(void *))(image+0x31c7b0))(resource);return resource;
}
static void *turbo_resource(void *self,void **out){
    void *result=turbo_resource_next(self,out);TurboResource *original=out?*out:NULL;
    if(!InterlockedExchange(&turbo_resource_seen,1))log_line("TURBO MENU: resource hook invoked; watching tuning page streams.\r\n");
    if(!turbo_ready||!original||!readable(original,sizeof(*original))||original->length<=0||original->length>=1048576||!readable(original->bytes,(size_t)original->length))return result;
    size_t reported=(size_t)original->length;
    char *text=HeapAlloc(GetProcessHeap(),0,reported+1);if(!text)return result;
    size_t text_length=turbo_copy_text(original->bytes,reported,text,reported+1);
    if(!text_length){HeapFree(GetProcessHeap(),0,text);return result;}
    if(!strstr(text,"ForzaUI.Scenes.Anthem.A_TuningOptions")){
        if(strstr(text,"F545_ChassisTuning")&&!InterlockedExchange(&turbo_legacy_seen,1))log_line("TURBO MENU: legacy F545 tuning layout encountered; modern page not detected.\r\n");
        HeapFree(GetProcessHeap(),0,text);return result;
    }
    log_line("TURBO MENU: modern Tune Car stream detected; bounded text copy ready.\r\n");
    size_t capacity=text_length+sizeof(turbo_resources)+sizeof(turbo_panel)+sizeof(turbo_state)+sizeof(turbo_help)+128;
    TurboResource *made=((void *(*)(size_t))(image+0x3229c0))(sizeof(*made)+capacity);
    if(!made){HeapFree(GetProcessHeap(),0,text);return result;}
    char *bytes=(char *)(made+1);size_t length=turbo_edit_xaml(text,text_length,bytes,capacity);
    HeapFree(GetProcessHeap(),0,text);
    if(!length){((void (*)(void *))(image+0x31c7b0))(made);log_line("TURBO MENU: XAML anchors mismatch; native page retained.\r\n");return result;}
    ((void (*)(void *))(image+0x143eb80))(made);
    memcpy(turbo_resource_vtable,original->vtable-1,sizeof(turbo_resource_vtable));turbo_resource_vtable[1]=turbo_delete_resource;
    made->vtable=turbo_resource_vtable+1;made->bytes=bytes;made->length=(int)length;made->capacity=(int)capacity;
    *out=made;turbo_release(original);InterlockedExchange(&turbo_xaml_ready,1);
    log_line("TURBO MENU: Tune Car page extended with Turbo panel.\r\n");return result;
}
static int turbo_install(size_t size){
    for(unsigned i=0;i<sizeof(turbo_signatures)/sizeof(*turbo_signatures);i++)if(memcmp(image+turbo_signatures[i].rva,turbo_signatures[i].bytes,16)){
        log_line("TURBO MENU: native signature mismatch; menu disabled, anti-lag profiles remain available.\r\n");return 0;
    }
    if(!turbo_register()){log_line("TURBO MENU: property registration refused.\r\n");return 0;}
    const unsigned rvas[]={0xe73960,0xe94db0,0xe93ec0,0xe90cd0,0x15f8a70,0xe8a890,0x349fc0},lengths[]={15,10,15,9,10,8,15};
    void *handlers[]={turbo_factory,turbo_change,turbo_update,turbo_init,turbo_dependency,turbo_advance,turbo_defaults};void **next[]={ (void **)&turbo_factory_next,(void **)&turbo_change_next,(void **)&turbo_update_next,(void **)&turbo_init_next,(void **)&turbo_dependency_next,(void **)&turbo_advance_next,(void **)&turbo_defaults_next};
    for(unsigned h=0;h<7;h++){
        const unsigned char *expected=NULL;
        for(unsigned i=0;i<sizeof(turbo_signatures)/sizeof(*turbo_signatures);i++)if(turbo_signatures[i].rva==rvas[h])expected=turbo_signatures[i].bytes;
        if(!expected||!entry_hook_install(image,size,rvas[h],expected,lengths[h],handlers[h],next[h]))return 0;
    }
    if(!entry_hook_chain_thunk(image,size,0x13fe354,turbo_resource,(void **)&turbo_resource_next))return 0;
    InterlockedExchange(&turbo_ready,1);log_line("TURBO MENU: hooks installed; awaiting Tune Car resources.\r\n");return 1;
}
