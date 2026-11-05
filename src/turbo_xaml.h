/* GPL-3.0. Scoped, additive edits to the native Tune Car page. */
#include "turbo_fragments.h"
/* Native streams may include trailing zero padding, or have no terminator.
   Copy only the reported bytes; never inspect one byte past the stream. */
static size_t turbo_copy_text(const char *bytes,size_t length,char *out,size_t capacity){
    if(!bytes||!out||!length||length>=1048576||capacity<=length)return 0;
    const char *zero=memchr(bytes,0,length);size_t used=zero?(size_t)(zero-bytes):length;
    if(zero)for(size_t i=used;i<length;i++)if(bytes[i])return 0;
    if(!used)return 0;memcpy(out,bytes,used);out[used]=0;return used;
}
static const char *ui_unique(const char *text,const char *needle){
    const char *at=strstr(text,needle);return at&&!strstr(at+1,needle)?at:NULL;
}
static size_t turbo_edit_xaml(const char *text,size_t length,char *out,size_t capacity){
    if(!text||!out||length>=1048576||text[length]!=0||strlen(text)!=length||!ui_unique(text,"x:Class=\"ForzaUI.Scenes.Anthem.A_TuningOptions\"")||strstr(text,"ALS_TurboPanel"))return 0;
    const char *resources=ui_unique(text,"</Page.Resources>");
    if(!resources)return 0;
    const char *dict=NULL,*p=text;while((p=strstr(p,"</ResourceDictionary>")) && p<resources){dict=p;p++;}
    const char *state=ui_unique(text,"<VisualState x:Name=\"VisualState_Tires\"");
    const char *panel=ui_unique(text,"<local:CommonListBox x:Name=\"StackPanel_1_Tires\"");
    const char *help=ui_unique(text,"<local:AutoScrollPanel DataContext=\"{Binding SelectedItem, ElementName=StackPanel_1_Tires}\"");
    if(!dict||!state||!panel||!help||!(dict<state&&state<panel&&panel<help))return 0;
    const char *root=ui_unique(text,"x:Class=\"ForzaUI.Scenes.Anthem.A_TuningOptions\"");
    if(strstr(text,"ALS_TuningPage"))return 0;
    root+=strlen("x:Class=\"ForzaUI.Scenes.Anthem.A_TuningOptions\"");
    const char *at[]={root,dict,state,panel,help};const char *insert[]={" x:Name=\"ALS_TuningPage\"",turbo_resources,turbo_state,turbo_panel,turbo_help};
    size_t total=length;for(unsigned i=0;i<5;i++)total+=strlen(insert[i]);if(total+1>capacity)return 0;
    size_t offset=0,written=0;
    for(unsigned i=0;i<5;i++){size_t next=(size_t)(at[i]-text),n=next-offset;memcpy(out+written,text+offset,n);written+=n;n=strlen(insert[i]);memcpy(out+written,insert[i],n);written+=n;offset=next;}
    memcpy(out+written,text+offset,length-offset);out[total]=0;return total;
}
