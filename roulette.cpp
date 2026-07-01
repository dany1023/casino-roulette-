#include "raylib.h"
#include <cmath>
#include <string>
#include <algorithm>
#include <vector>
#include <map>
#include <cstring>
#include <cstdint>
//compilazione statica
//g++ r1_fixed.cpp -o g.exe -O2 -static -static-libgcc -static-libstdc++ -I E:/c++/roulette -L E:/c++/roulette -lraylib -lopengl32 -lgdi32 -lwinmm -mwindows



// ================================================================
// TIPI
// ================================================================
struct Puntata{
    int tipo,valore;
    int ns[4]={0,0,0,0};
    std::string desc;
};

struct Roulette{
    float ang=0,vel=0;
    bool gira=false;
    float attrito=0.985f;
    int ris=-1;
    const char**nomi;Color*col;
};

struct Cella{Rectangle r;int tipo;std::string label;Color bg;};

struct SelMult{
    bool attiva=false;
    int tipo=0;   
    int valore=0;
    int numeri[4]={0,0,0,0};
    int idxRuota[4]={-1,-1,-1,-1};
    int cnt=0;
};

struct Giocatore{
    int wallet=0,tipoPuntata=-1,importoTesto=0;
    std::string buf="";
    int ns[4]={0,0,0,0},cntNs=0;
    int fase=-2;
    std::string msg="INSERISCI BILANCIO INIZIALE E PREMI INVIO";
    std::vector<Puntata> puntate;
    int ficheVal=10;
    int ficheList[5]={1,5,10,50,100};
    bool testoMode=false; 
    SelMult sel;
};

// ================================================================
// HELPERS
// ================================================================
Color CfVal(int v){
    if(v==1)  return Color{210,210,210,255};
    if(v==5)  return RED;
    if(v==10) return BLUE;
    if(v==50) return Color{130,0,130,255};
    if(v==100)return Color{255,150,0,255};
    return GRAY;
}
bool possibile2(int a,int b){if(a>b)std::swap(a,b);return b==a+1||b==a+3;}
bool possibile4(int a[]){std::sort(a,a+4);return a[1]==a[0]+1&&a[2]==a[0]+3&&a[3]==a[0]+4;}

int TotPuntato(const Giocatore&g){
    int t=0;for(auto&p:g.puntate)t+=p.valore;return t;
}

std::string Desc(int tipo,int ns[4]=nullptr){
    if(tipo==19)return"00";
    if(tipo==0) return"N.0";
    if(tipo<=37){
        // tipo è l'indice nella ruota, non il numero diretto
        // il numero reale viene dal label della cella, ma qui usiamo desc generica
        return"N."+std::to_string(tipo);
    }
    const char*n[]={"1-12","13-24","25-36","1-18","19-36",
        "Pari","Disp","Rosso","Nero","Riga1","Riga2","Riga3"};
    if(tipo>=38&&tipo<=49)return n[tipo-38];
    if(tipo==50&&ns)return"Cavallo "+std::to_string(ns[0])+"-"+std::to_string(ns[1]);
    if(tipo==51&&ns)return"Quartina "+std::to_string(ns[0])+"/"+std::to_string(ns[1])+"/"+std::to_string(ns[2])+"/"+std::to_string(ns[3]);
    if(tipo>=47&&tipo<=49)return n[tipo-38];
    return"?";
}

int NumDaIdx(int idx,const char*nr[]){return idx==19?37:atoi(nr[idx]);}

void SistemaFiche(Giocatore&g){
    if(g.ficheVal<=g.wallet)return;
    int b=0;
    for(int i=0;i<5;i++)if(g.ficheList[i]<=g.wallet&&g.ficheList[i]>b)b=g.ficheList[i];
    if(b>0)g.ficheVal=b;
}

void Aggiungi(Giocatore&g,std::map<int,int>&fst,int tipo,int val,int ns[4],const std::string&customDesc){
    if(val<=0||val>g.wallet)return;
    bool merged=false;
    if(tipo<50){
        for(auto&p:g.puntate)if(p.tipo==tipo){p.valore+=val;merged=true;break;}
    } else {
        int cnt=(tipo==50)?2:4;
        int sNs[4]={0,0,0,0};
        if(ns){memcpy(sNs,ns,cnt*sizeof(int));std::sort(sNs,sNs+cnt);}
        for(auto&p:g.puntate){
            if(p.tipo!=tipo)continue;
            int pNs[4]={p.ns[0],p.ns[1],p.ns[2],p.ns[3]};
            std::sort(pNs,pNs+cnt);
            bool same=true;
            for(int i=0;i<cnt;i++)if(pNs[i]!=sNs[i]){same=false;break;}
            if(same){p.valore+=val;merged=true;break;}
        }
    }
    if(!merged){
        Puntata p;p.tipo=tipo;p.valore=val;
        memset(p.ns,0,sizeof(p.ns));
        if(ns)memcpy(p.ns,ns,sizeof(p.ns));
        p.desc=customDesc.empty()?Desc(tipo,ns):customDesc;
        g.puntate.push_back(p);
    }
    g.wallet-=val;fst[tipo]+=val;
    SistemaFiche(g);
}

void Aggiungi(Giocatore&g,std::map<int,int>&fst,int tipo,int val,int ns[4]=nullptr){
    Aggiungi(g,fst,tipo,val,ns,std::string(""));
}

void Annulla(Giocatore&g,std::map<int,int>&fst){
    if(g.puntate.empty())return;
    auto&u=g.puntate.back();
    g.wallet+=u.valore;
    fst[u.tipo]-=u.valore;
    if(fst[u.tipo]<=0)fst.erase(u.tipo);
    g.puntate.pop_back();
    SistemaFiche(g);
}

struct TabInfo{std::vector<Cella>celle;int n2idx[37]={};};

Color CNumTab(int n){
    static const int r[]={1,3,5,7,9,12,14,16,18,19,21,23,25,27,30,32,34,36};
    for(int x:r)if(n==x)return Color{160,22,22,255};
    return Color{18,18,18,255};
}

TabInfo CreaTab(float x,float y,float w,float h,const char*nr[]){
    TabInfo t;
    for(int n=1;n<=36;n++)for(int i=0;i<38;i++)if(i!=19&&atoi(nr[i])==n){t.n2idx[n]=i;break;}
    float zW=w*0.053f,c2W=w*0.058f,nW=(w-zW-c2W)/12.0f;
    float sH=h*0.165f,nH=h-2*sH,rH=nH/3.0f;
    for(int col=0;col<12;col++)for(int row=0;row<3;row++){
        int num=col*3+(3-row);
        t.celle.push_back({{x+zW+col*nW,y+row*rH,nW,rH},t.n2idx[num],std::to_string(num),CNumTab(num)});
    }
    t.celle.push_back({{x,y,zW,nH/2},0,"0",Color{0,120,0,255}});
    t.celle.push_back({{x,y+nH/2,zW,nH/2},19,"00",Color{0,120,0,255}});
    for(int r=0;r<3;r++)t.celle.push_back({{x+zW+12*nW,y+r*rH,c2W,rH},47+r,"2:1",Color{20,75,20,255}});
    float dX=x+zW,dW=12*nW/3,dY=y+nH;
    const char*dl[]={"1a Dozzina","2a Dozzina","3a Dozzina"};
    for(int i=0;i<3;i++)t.celle.push_back({{dX+i*dW,dY,dW,sH},38+i,dl[i],Color{20,75,20,255}});
    float bY=dY+sH,bW=12*nW/6;
    struct B{int t;const char*l;Color c;};
    B b[]={{41,"1-18",Color{20,75,20,255}},{43,"PARI",Color{20,75,20,255}},
           {45,"ROSSO",Color{160,22,22,255}},{46,"NERO",Color{18,18,18,255}},
           {44,"DISPARI",Color{20,75,20,255}},{42,"19-36",Color{20,75,20,255}}};
    for(int i=0;i<6;i++)t.celle.push_back({{dX+i*bW,bY,bW,sH},b[i].t,b[i].l,b[i].c});
    return t;
}


// DISEGNO FICHE
void Fiche(float cx,float cy,float rad,Color base,bool sel){
    Color dk={uint8_t(base.r/2),uint8_t(base.g/2),uint8_t(base.b/2),255};
    Color lt={uint8_t(std::min(255,(int)base.r+55)),uint8_t(std::min(255,(int)base.g+55)),uint8_t(std::min(255,(int)base.b+55)),255};
    DrawCircle(cx,cy,rad,base);
    for(int s=0;s<8;s++){
        float a0=s*45*(PI/180),a1=(s*45+18)*(PI/180);
        float ri=rad*.78f,ro=rad*.96f;
        Vector2 p0={cx+cosf(a0)*ri,cy+sinf(a0)*ri},p1={cx+cosf(a0)*ro,cy+sinf(a0)*ro},
                p2={cx+cosf(a1)*ro,cy+sinf(a1)*ro},p3={cx+cosf(a1)*ri,cy+sinf(a1)*ri};
        DrawTriangle(p0,p1,p2,lt);DrawTriangle(p0,p2,p3,lt);
    }
    DrawCircleLines(cx,cy,rad*.68f,Fade(WHITE,.35f));
    if(sel){DrawCircle(cx,cy,rad+4,Fade(YELLOW,.25f));DrawCircleLines(cx,cy,rad+4,YELLOW);}
    DrawCircleLines(cx,cy,rad,sel?YELLOW:dk);
}

// VINCITE
int Vinci(int nvIdx,const Puntata&p,const char*nr[],Color col[]){
    int nv=nvIdx==19?37:atoi(nr[nvIdx]);
    int b1[]={3,6,9,12,15,18,21,24,27,30,33,36};
    int b2[]={2,5,8,11,14,17,20,23,26,29,32,35};
    int b3[]={1,4,7,10,13,16,19,22,25,28,31,34};
    if(p.tipo<=37)return p.tipo==nvIdx?p.valore*36:0;
    switch(p.tipo){
    case 38:if(nv>=1&&nv<=12)return p.valore*3;break;
    case 39:if(nv>=13&&nv<=24)return p.valore*3;break;
    case 40:if(nv>=25&&nv<=36)return p.valore*3;break;
    case 41:if(nv>=1&&nv<=18)return p.valore*2;break;
    case 42:if(nv>=19&&nv<=36)return p.valore*2;break;
    case 43:if(nvIdx!=0&&nvIdx!=19&&nv%2==0)return p.valore*2;break;
    case 44:if(nvIdx!=0&&nvIdx!=19&&nv%2!=0)return p.valore*2;break;
    case 45:if(nvIdx!=0&&nvIdx!=19&&col[nvIdx].r>100)return p.valore*2;break;
    case 46:if(nvIdx!=0&&nvIdx!=19&&col[nvIdx].r<50&&col[nvIdx].g<50)return p.valore*2;break;
    case 47:for(int n:b1)if(nv==n)return p.valore*3;break;
    case 48:for(int n:b2)if(nv==n)return p.valore*3;break;
    case 49:for(int n:b3)if(nv==n)return p.valore*3;break;
    case 50:if(nv==p.ns[0]||nv==p.ns[1])return p.valore*17;break;
    case 51:for(int i=0;i<4;i++)if(nv==p.ns[i])return p.valore*8;break;
    }
    return 0;
}


int main(){
    InitWindow(0,0,"American Roulette");
    ToggleFullscreen();
    SetTargetFPS(60);

    const char*nr[38]={"0","28","9","26","30","11","7","20","32","17","5","22","34","15","3","24","36","13","1","00","27","10","25","29","12","8","19","31","18","6","21","33","16","4","23","35","14","2"};
    Color col[38]={GREEN,BLACK,RED,BLACK,RED,BLACK,RED,BLACK,RED,BLACK,RED,BLACK,RED,BLACK,RED,BLACK,RED,BLACK,RED,GREEN,RED,BLACK,RED,BLACK,RED,BLACK,RED,BLACK,RED,BLACK,RED,BLACK,RED,BLACK,RED,BLACK,RED,BLACK};

    Roulette rou={0,0,false,0.985f,-1,nr,col};
    Giocatore g;
    std::map<int,int>fst; 

    while(!WindowShouldClose()){
        int SW=GetScreenWidth(),SH=GetScreenHeight();
        float passo=360.0f/38.0f;
        const int M=14; 

        int CL_W=SW*0.38f;
        float INFO_H_L=115.0f;
        float rou_area=SH-INFO_H_L-M*2;
        float raggio=std::min((CL_W-M*2)*0.47f, rou_area*0.47f);
        Vector2 centro={(float)(M+(CL_W-M*2)/2), (float)(M+raggio*1.06f)};
        int INFO_Y_L=(int)(centro.y+raggio*1.1f);

        int CR_X=CL_W+M;
        int CR_W=SW-CR_X-M;
        int TAB_X=CR_X, TAB_Y=M;
        int TAB_W=CR_W, TAB_H=SH*0.55f;
        int PNL_X=CR_X, PNL_Y=TAB_Y+TAB_H+M;
        int PNL_W=CR_W, PNL_H=SH-PNL_Y-M;

        TabInfo tab=CreaTab(TAB_X,TAB_Y,TAB_W,TAB_H,nr);

        // Misure Tabellone per click intersezioni
        float zW=TAB_W*0.053f, c2W=TAB_W*0.058f, nW=(TAB_W-zW-c2W)/12.0f;
        float sH=TAB_H*0.165f, nH=TAB_H-2*sH, rH=nH/3.0f;
        float gridX=TAB_X+zW, gridY=TAB_Y;

        if(!rou.gira){
            if(g.fase==-2){
                int key=GetCharPressed();
                while(key>0&&g.buf.size()<10){if(key>='0'&&key<='9')g.buf+=(char)key;key=GetCharPressed();}
                if(IsKeyPressed(KEY_BACKSPACE)&&!g.buf.empty())g.buf.pop_back();
                if(IsKeyPressed(KEY_ENTER)&&!g.buf.empty()){
                    int v=std::stoi(g.buf);g.buf="";
                    if(v>0){g.wallet=v;g.fase=-1;g.testoMode=false;SistemaFiche(g);g.msg="Clicca sul tabellone per puntare!";}
                    else g.msg="Inserisci un valore valido!";
                }
            }
            else if(!g.testoMode){
                if(IsKeyPressed(KEY_TAB)){
                    g.testoMode=true;
                    g.buf=std::to_string(g.ficheVal); 
                    g.fase=-1;
                    g.msg="Inserisci importo (preimpostato su fiche):";
                }
                if(IsKeyPressed(KEY_ONE))  g.ficheVal=g.ficheList[0];
                if(IsKeyPressed(KEY_TWO))  g.ficheVal=g.ficheList[1];
                if(IsKeyPressed(KEY_THREE))g.ficheVal=g.ficheList[2];
                if(IsKeyPressed(KEY_FOUR)) g.ficheVal=g.ficheList[3];
                if(IsKeyPressed(KEY_FIVE)) g.ficheVal=g.ficheList[4];
                if(IsKeyPressed(KEY_ENTER)&&!g.puntate.empty()&&!g.sel.attiva)
                    {rou.gira=true;rou.vel=(float)GetRandomValue(250,400)/100.0f;g.msg="La roulette gira!";}
                if(IsKeyPressed(KEY_ESCAPE)&&g.sel.attiva){g.sel={};g.msg="Selezione annullata.";}
                if(IsKeyPressed(KEY_Z)&&!g.sel.attiva){Annulla(g,fst);g.msg="Ultima puntata annullata.";}

                if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
                    Vector2 mouse=GetMousePosition();
                    int fR=20,fY=PNL_Y+M+fR;
                    for(int i=0;i<5;i++){
                        int fx=PNL_X+M+i*(fR*2+10)+fR;
                        if(CheckCollisionPointCircle(mouse,{(float)fx,(float)fY},(float)(fR+7)))
                            g.ficheVal=g.ficheList[i];
                    }
                    bool puntataPiazzata = false;

                    // CONTROLLO CLICK INTERSEZIONI (Cavallo / Quartina)
                    if (!g.sel.attiva && mouse.x >= gridX && mouse.x <= gridX + 12*nW && mouse.y >= gridY && mouse.y <= gridY + 3*rH) {
                        float fx = (mouse.x - gridX) / nW;
                        float fy = (mouse.y - gridY) / rH;
                        int cx = (int)round(fx);
                        int cy = (int)round(fy);
                        float dx = std::abs(fx - cx) * nW;
                        float dy = std::abs(fy - cy) * rH;
                        float T = 15.0f; // Tolleranza in pixel per il click

                        bool isQ = (cx >= 1 && cx <= 11 && cy >= 1 && cy <= 2 && dx < T && dy < T);
                        bool isCV = (!isQ && cy >= 1 && cy <= 2 && dy < T);
                        bool isCH = (!isQ && cx >= 1 && cx <= 11 && dx < T);

                        if (isQ || isCV || isCH) {
                            puntataPiazzata = true;
                            int costo = g.ficheVal;
                            if (costo > g.wallet) { SistemaFiche(g); costo = g.ficheVal; }
                            
                            if (costo <= g.wallet && costo > 0) {
                                if (isQ) {
                                    int ns[4] = { (cx-1)*3+(4-cy), cx*3+(4-cy), (cx-1)*3+(3-cy), cx*3+(3-cy) };
                                    Aggiungi(g, fst, 51, costo, ns);
                                    g.msg = TextFormat("Quartina piazzata! Tot: %d$", TotPuntato(g));
                                } else if (isCV) {
                                    int col = (int)floor(fx);
                                    int ns[4] = { col*3+(4-cy), col*3+(3-cy), 0, 0 };
                                    Aggiungi(g, fst, 50, costo, ns);
                                    g.msg = TextFormat("Cavallo piazzato! Tot: %d$", TotPuntato(g));
                                } else if (isCH) {
                                    int row = (int)floor(fy);
                                    int ns[4] = { (cx-1)*3+(3-row), cx*3+(3-row), 0, 0 };
                                    Aggiungi(g, fst, 50, costo, ns);
                                    g.msg = TextFormat("Cavallo piazzato! Tot: %d$", TotPuntato(g));
                                }
                            } else {
                                g.msg = "Credito insufficiente!";
                            }
                        }
                    }

                    // CONTROLLO CLICK CELLE STANDARD
                    if (!puntataPiazzata) {
                        for(auto&cella:tab.celle){
                            if(!CheckCollisionPointRec(mouse,cella.r))continue;
                            int costo=g.ficheVal;
                            if(costo>g.wallet){SistemaFiche(g);costo=g.ficheVal;}
                            if(costo<=0||costo>g.wallet){g.msg="Credito insufficiente!";break;}

                            if(g.sel.attiva){
                                if(cella.tipo>37){g.msg="Seleziona un numero singolo!";break;}
                                int num=NumDaIdx(cella.tipo,nr);
                                bool dup=false;
                                for(int k=0;k<g.sel.cnt;k++)if(g.sel.numeri[k]==num){dup=true;break;}
                                if(dup){g.msg="Numero gia' selezionato!";break;}
                                g.sel.numeri[g.sel.cnt]=num;
                                g.sel.idxRuota[g.sel.cnt]=cella.tipo;
                                g.sel.cnt++;
                                int lim=g.sel.tipo==50?2:4;
                                if(g.sel.cnt<lim){
                                    g.msg=TextFormat("Seleziona %d° numero (%d/%d) | ESC=annulla",g.sel.cnt+1,g.sel.cnt,lim);
                                } else {
                                    bool ok=(g.sel.tipo==50&&possibile2(g.sel.numeri[0],g.sel.numeri[1]))||
                                            (g.sel.tipo==51&&possibile4(g.sel.numeri));
                                    if(ok){
                                        Aggiungi(g,fst,g.sel.tipo,g.sel.valore,g.sel.numeri);
                                        g.msg=TextFormat("%s aggiunta! Tot: %d$",g.sel.tipo==50?"Cavallo":"Quartina",TotPuntato(g));
                                    } else {
                                        g.msg="Numeri non adiacenti! Riprova.";
                                    }
                                    g.sel={};
                                }
                            } else {
                                Aggiungi(g,fst,cella.tipo,costo,nullptr,"N."+cella.label);
                                g.msg=TextFormat("%d$ su %s | Tot: %d$ | [INVIO]=Gira",costo,cella.label.c_str(),TotPuntato(g));
                            }
                            break;
                        }
                    }
                }
            }
            else{
                if(IsKeyPressed(KEY_TAB)){g.testoMode=false;g.fase=-1;g.buf="";g.msg="Modalita click.";}
                int key=GetCharPressed();
                while(key>0&&g.buf.size()<10){if(key>='0'&&key<='9')g.buf+=(char)key;key=GetCharPressed();}
                if(IsKeyPressed(KEY_BACKSPACE)&&!g.buf.empty())g.buf.pop_back();
                if(IsKeyPressed(KEY_Z)&&g.fase==-1){Annulla(g,fst);g.msg="Ultima puntata annullata.";}
                if(IsKeyPressed(KEY_ENTER)){
                    if(!g.buf.empty()){
                        int v=std::stoi(g.buf);g.buf="";
                        if(g.fase==-1){
                            if(v>0&&v<=g.wallet){g.importoTesto=v;g.fase=0;g.msg="Tipo puntata (0-51):";}
                            else g.msg="Importo non valido!";
                        } else if(g.fase==0){
                            g.tipoPuntata=v;
                            if(v==50||v==51){g.fase=1;g.cntNs=0;g.msg="1° numero:";}
                            else if(v>=1&&v<=36){
                                // v è il NUMERO REALE: convertiamo in indice ruota usando n2idx
                                int idx=tab.n2idx[v];
                                Aggiungi(g,fst,idx,g.importoTesto,nullptr,"N."+std::to_string(v));
                                g.fase=-1;g.msg="Aggiunta! Altro importo o INVIO=Gira";
                            }
                            else if(v==0){Aggiungi(g,fst,0,g.importoTesto,nullptr,"N.0");g.fase=-1;g.msg="Aggiunta! Altro importo o INVIO=Gira";}
                            else if(v==37){Aggiungi(g,fst,19,g.importoTesto,nullptr,"N.00");g.fase=-1;g.msg="Aggiunta! Altro importo o INVIO=Gira";}
                            else if(v>=38&&v<=49){Aggiungi(g,fst,v,g.importoTesto);g.fase=-1;g.msg="Aggiunta! Altro importo o INVIO=Gira";}
                            else g.msg="Tipo non valido (0-51)!";
                        } else if(g.fase=1){
                            g.ns[g.cntNs++]=v;
                            int lim=g.tipoPuntata==50?2:4;
                            if(g.cntNs>=lim){
                                bool ok=(g.tipoPuntata==50&&possibile2(g.ns[0],g.ns[1]))||
                                        (g.tipoPuntata==51&&possibile4(g.ns));
                                if(ok){Aggiungi(g,fst,g.tipoPuntata,g.importoTesto,g.ns);g.fase=-1;g.msg="Aggiunta! Altro importo o INVIO=Gira";}
                                else{g.cntNs=0;g.msg="Non adiacenti! Riprova 1° numero:";}
                            } else g.msg=TextFormat("%d° numero:",g.cntNs+1);
                        }
                    } else {
                        if(!g.puntate.empty()){rou.gira=true;rou.vel=(float)GetRandomValue(250,400)/100.0f;g.msg="La roulette gira!";}
                    }
                }
            }
            if(g.wallet==0&&g.puntate.empty()&&g.fase!=-2){
                g.fase=-2;g.testoMode=false;g.buf="";g.sel={};
                g.msg="GAME OVER! Inserisci nuovo bilancio:";
            }
        }

        if(rou.gira){
            rou.ang+=rou.vel;rou.vel*=rou.attrito;
            if(rou.vel<0.01f){
                rou.gira=false;
                float gradi=fmodf(rou.ang*(180/PI),360);if(gradi<0)gradi+=360;
                int idx=(int)(gradi/passo)%38;
                rou.ang=(idx*passo+passo/2)*(PI/180);
                rou.ris=idx;
                int vt=0,pv=0;
                for(auto&p:g.puntate){int v=Vinci(idx,p,nr,col);if(v>0){vt+=v;pv++;}}
                g.puntate.clear();fst.clear();g.sel={};
                if(vt>0){g.wallet+=vt;g.msg=TextFormat("VINTO %d$ su %d puntate!",vt,pv);}
                else g.msg=TextFormat("Perso. Uscito: %s",nr[idx]);
                if(g.wallet>0){g.fase=-1;g.testoMode=false;SistemaFiche(g);}
                else{g.fase=-2;g.testoMode=false;g.buf="";g.msg="GAME OVER! Inserisci nuovo bilancio:";}
            }
        }

        BeginDrawing();
        ClearBackground(Color{12,52,10,255});
        
        DrawCircleV(centro,raggio*1.06f,DARKGRAY);
        DrawCircleLinesV(centro,raggio*1.06f,GOLD);
        for(int i=0;i<38;i++){
            DrawCircleSector(centro,raggio,i*passo,(i+1)*passo,40,col[i]);
            float rad2=(i*passo+passo/2)*(PI/180);
            Vector2 pn={centro.x+cosf(rad2)*(raggio*.88f),centro.y+sinf(rad2)*(raggio*.88f)};
            DrawText(nr[i],(int)(pn.x-10),(int)(pn.y-10),(int)(raggio*.07f),WHITE);
        }
        DrawCircleV(centro,raggio*.65f,Color{65,32,7,255});
        Vector2 pp={centro.x+cosf(rou.ang)*(raggio*.72f),centro.y+sinf(rou.ang)*(raggio*.72f)};
        DrawCircleV(pp,raggio*.04f,RAYWHITE);
        DrawRectangle(M,INFO_Y_L,CL_W-M*2,(int)INFO_H_L+5,Color{0,0,0,100});
        DrawText("[INVIO]=Gira la routa  [Z]=Annulla Puntata",M+8, INFO_Y_L+8, 20,WHITE);
        DrawText("[TAB]=Cambia modalita fiches/testo",M+8, INFO_Y_L+32, 20,WHITE);
        DrawText(TextFormat("BILANCIO: %d$",g.wallet),     M+8, INFO_Y_L+54,  20, GREEN);
        DrawText(TextFormat("PUNTATO:  %d$",TotPuntato(g)),M+8, INFO_Y_L+76, 20, ORANGE);
        if(rou.ris!=-1&&!rou.gira)
            DrawText(TextFormat("Uscito: %s",nr[rou.ris]), M+8, INFO_Y_L+98, 20, YELLOW);

        {
            Vector2 mouse=GetMousePosition();
            int hovTipo=-1;
            // Tipi delle celle da evidenziare al hover (cavallo=2, quartina=4 celle)
            std::vector<int> hovTipi; // indici ruota delle celle evidenziate
            bool hovIsIntersect=false; // hover su intersezione?

            if(!rou.gira&&!g.testoMode&&g.fase==-1){
                // Check intersezioni PRIMA delle celle (hanno priorità visiva)
                float fx=(mouse.x-gridX)/nW;
                float fy=(mouse.y-gridY)/rH;
                int cx=(int)round(fx);
                int cy=(int)round(fy);
                float dxp=std::abs(fx-cx)*nW;
                float dyp=std::abs(fy-cy)*rH;
                float T=15.0f;
                bool inGrid=(mouse.x>=gridX&&mouse.x<=gridX+12*nW&&mouse.y>=gridY&&mouse.y<=gridY+3*rH);

                if(inGrid){
                    bool isQ=(cx>=1&&cx<=11&&cy>=1&&cy<=2&&dxp<T&&dyp<T);
                    bool isCV=(!isQ&&cy>=1&&cy<=2&&dyp<T&&mouse.x>=gridX&&mouse.x<=gridX+12*nW);
                    bool isCH=(!isQ&&cx>=1&&cx<=11&&dxp<T&&mouse.y>=gridY&&mouse.y<=gridY+3*rH);

                    if(isQ){
                        hovIsIntersect=true;
                        int n1=(cx-1)*3+(4-cy),n2=cx*3+(4-cy),n3=(cx-1)*3+(3-cy),n4=cx*3+(3-cy);
                        int ns4[]={n1,n2,n3,n4};
                        for(int n:ns4)for(auto&c:tab.celle)if(c.tipo==tab.n2idx[n]){hovTipi.push_back(c.tipo);break;}
                    } else if(isCV){
                        hovIsIntersect=true;
                        int col2=(int)floor(fx);
                        int na=col2*3+(4-cy),nb=col2*3+(3-cy);
                        for(int n:{na,nb})for(auto&c:tab.celle)if(c.tipo==tab.n2idx[n]){hovTipi.push_back(c.tipo);break;}
                    } else if(isCH){
                        hovIsIntersect=true;
                        int row=(int)floor(fy);
                        int na=(cx-1)*3+(3-row),nb=cx*3+(3-row);
                        for(int n:{na,nb})for(auto&c:tab.celle)if(c.tipo==tab.n2idx[n]){hovTipi.push_back(c.tipo);break;}
                    }
                }
                if(!hovIsIntersect)
                    for(auto&c:tab.celle)if(CheckCollisionPointRec(mouse,c.r)){hovTipo=c.tipo;break;}
            }

            // PASSO 1: Disegno gli sfondi e i testi di tutte le celle (nessuna sovrapposizione di fiches qui)
            for(auto&c:tab.celle){
                bool hov=(hovIsIntersect)?false:(c.tipo==hovTipo);
                bool hovMult=false;
                for(int t:hovTipi)if(t==c.tipo){hovMult=true;break;}
                bool inSel=false;
                for(int k=0;k<g.sel.cnt;k++)if(g.sel.idxRuota[k]==c.tipo){inSel=true;break;}
                Color bg=c.bg;
                if(inSel)           bg=Color{190,165,0,255};
                else if(hovMult)    bg=Color{uint8_t(std::min(255,(int)bg.r+80)),uint8_t(std::min(255,(int)bg.g+80)),uint8_t(std::min(255,(int)bg.b+30)),255};
                else if(hov)        bg=Color{uint8_t(std::min(255,(int)bg.r+55)),uint8_t(std::min(255,(int)bg.g+55)),uint8_t(std::min(255,(int)bg.b+55)),255};
                DrawRectangleRec(c.r,bg);
                DrawRectangleLinesEx(c.r,(inSel||hov||hovMult)?2.5f:1.0f,inSel?GOLD:((hov||hovMult)?YELLOW:Color{62,62,62,255}));
                
                int fs=std::min((int)(c.r.width*.36f),(int)(c.r.height*.40f));
                if(fs<7)fs=7;if(fs>20)fs=20;
                int tw=MeasureText(c.label.c_str(),fs);
                DrawText(c.label.c_str(),(int)(c.r.x+c.r.width/2-tw/2),(int)(c.r.y+c.r.height/2-fs/2),fs,WHITE);
            }

            // PASSO 2: Calcolo posizione e disegno di tutte le fiches (SOPRA le celle)
            struct ChipToDraw { float cx, cy; int tot; };
            std::map<std::string, ChipToDraw> toDraw;

            for (auto& p : g.puntate) {
                std::string key;
                if (p.tipo <= 49) {
                    key = std::to_string(p.tipo);
                } else {
                    int sNs[4] = {p.ns[0], p.ns[1], p.ns[2], p.ns[3]};
                    std::sort(sNs, sNs + (p.tipo == 50 ? 2 : 4));
                    key = std::to_string(p.tipo) + "_" + std::to_string(sNs[0]) + "_" + std::to_string(sNs[1]);
                    if(p.tipo == 51) key += "_" + std::to_string(sNs[2]) + "_" + std::to_string(sNs[3]);
                }

                if (toDraw.find(key) == toDraw.end()) {
                    float cx = 0, cy = 0;
                    if (p.tipo <= 49) {
                        Rectangle r = {0,0,0,0};
                        for(auto& c : tab.celle) if(c.tipo == p.tipo) { r = c.r; break; }
                        float rad = std::min(r.width, r.height) * 0.26f;
                        if(rad < 6) rad = 6; if(rad > 18) rad = 18;
                        cx = r.x + r.width - rad - 2;
                        cy = r.y + rad + 2;
                    } else if (p.tipo == 50) {
                        Rectangle r1 = {0,0,0,0}, r2 = {0,0,0,0};
                        int t1 = tab.n2idx[p.ns[0]], t2 = tab.n2idx[p.ns[1]];
                        for(auto& c : tab.celle) { if(c.tipo == t1) r1 = c.r; if(c.tipo == t2) r2 = c.r; }
                        cx = (r1.x + r1.width/2 + r2.x + r2.width/2) / 2.0f;
                        cy = (r1.y + r1.height/2 + r2.y + r2.height/2) / 2.0f;
                    } else if (p.tipo == 51) {
                        for(int k=0; k<4; k++) {
                            int t = tab.n2idx[p.ns[k]];
                            for(auto& c : tab.celle) if(c.tipo == t) { cx += c.r.x + c.r.width/2; cy += c.r.y + c.r.height/2; break; }
                        }
                        cx /= 4.0f; cy /= 4.0f;
                    }
                    toDraw[key] = {cx, cy, 0};
                }
                toDraw[key].tot += p.valore;
            }

            for (auto& kv : toDraw) {
                float cx = kv.second.cx, cy = kv.second.cy;
                int tot = kv.second.tot;
                float rad = 14.0f; // Misura adatta a stare tra più celle
                
                if(kv.first.find("_") == std::string::npos) {
                    int tipo = std::stoi(kv.first);
                    for(auto& c : tab.celle) if(c.tipo == tipo) {
                        rad = std::min(c.r.width, c.r.height) * 0.26f;
                        break;
                    }
                }
                if(rad < 8) rad = 8; if(rad > 18) rad = 18;

                // Disegno il cerchio base della Fiche
                Fiche(cx, cy, rad, CfVal(g.ficheVal), false);

                // Scritta INGRANDITA sulla fiche
                std::string ts = std::to_string(tot);
                int fs = std::max(13, (int)(rad * 1.5f)); // Font scalato al 150% del raggio
                if (fs > 24) fs = 24;

                int tw = MeasureText(ts.c_str(), fs);
                // Ombreggiatura nera sotto
                DrawText(ts.c_str(), (int)(cx - tw/2 + 1), (int)(cy - fs/2 + 1), fs, BLACK);
                // Testo principale bianco sopra (assicurando che la fiche non copra mai questo testo)
                DrawText(ts.c_str(), (int)(cx - tw/2), (int)(cy - fs/2), fs, WHITE);
            }
        }

        DrawRectangleRec({(float)PNL_X,(float)PNL_Y,(float)PNL_W,(float)PNL_H},Color{5,30,5,210});
        DrawRectangleLinesEx({(float)PNL_X,(float)PNL_Y,(float)PNL_W,(float)PNL_H},1,Fade(GOLD,.45f));
        DrawLine(CL_W,M,CL_W,SH-M,Fade(GOLD,.30f));

        if(g.fase==-2){
            int ty=PNL_Y+M;
            DrawText("INSERISCI BILANCIO INIZIALE",PNL_X+M,ty,20,GOLD);        ty+=24;
            DrawText(g.msg.c_str(),PNL_X+M,ty,20,ORANGE);                      ty+=24;
            DrawText(g.buf.c_str(),PNL_X+M,ty,30,YELLOW);

        } else if(!g.testoMode){
            int fR=20,fSpacing=fR*2+10;
            int fY=PNL_Y+M+fR;   
            for(int i=0;i<5;i++){
                int fx=PNL_X+M+i*fSpacing+fR;
                bool sel=g.ficheVal==g.ficheList[i];
                bool aff=g.ficheList[i]<=g.wallet;
                Fiche(fx,fY,fR,aff?CfVal(g.ficheList[i]):Fade(CfVal(g.ficheList[i]),.28f),sel&&aff);
                if(!aff){DrawCircle(fx,fY,fR,Fade(BLACK,.45f));}
                const char*lbl=TextFormat("%d",g.ficheList[i]);

                int lfs=std::max(14,(int)(fR*1.1f)); 
                int ltw=MeasureText(lbl,lfs);
                
                if(aff){DrawText(lbl,fx-ltw/2+1,fY-lfs/2+1,lfs,BLACK);}
                DrawText(lbl,fx-ltw/2,fY-lfs/2,lfs,aff?WHITE:Fade(WHITE,.3f));
                DrawText(TextFormat("[%d]",i+1),fx-8,fY+fR+4,20,Fade(WHITE,.5f));
            }

            int btnY=fY+fR+M+8;
            int btnH=26,btnW=85;

            int msgY=btnY;
            DrawText(g.msg.c_str(),PNL_X+M,msgY,20,ORANGE);

            int listY=msgY+16;
            if(!g.puntate.empty()){
                DrawText(TextFormat("Puntate (%d):",(int)g.puntate.size()),PNL_X+M,listY+10,20,GOLD);
                listY+=35;
                int lH=30;
                int disponibile=PNL_Y+PNL_H-listY-34; 
                int maxR=disponibile/lH; if(maxR<1)maxR=1;
                int start=(int)g.puntate.size()>maxR?(int)g.puntate.size()-maxR:0;
                for(int i=start;i<(int)g.puntate.size()&&listY+lH<PNL_Y+PNL_H-34;i++){
                    auto&p=g.puntate[i];
                    DrawText(TextFormat("  %s : %d$",p.desc.c_str(),p.valore),PNL_X+M,listY,20,LIGHTGRAY);
                    listY+=lH;
                }
            }
            DrawLine(PNL_X,PNL_Y+PNL_H-28,PNL_X+PNL_W,PNL_Y+PNL_H-28,Fade(GOLD,.3f));
            DrawText("[INVIO]=Gira  [Z]=Annulla  [TAB]=Testo  ",PNL_X+M,PNL_Y+PNL_H-17,11,Fade(WHITE,.55f));

        } else {
            int lx0=PNL_X+M, lx1=PNL_X+M+PNL_W/2;
            int ly=PNL_Y+M;
            int fs=20,lH=fs+3;
            DrawText("TIPI DI PUNTATA:",lx0,ly,fs+1,GOLD); ly+=lH+4;
            const char*voci[]={
                "0-36: Numero reale x36","37:   00(DoppioZero)x36",
                "38: 1-12 x3",      "39: 13-24 x3",
                "40: 25-36 x3",     "41: 1-18 x2",
                "42: 19-36 x2",     "43: Pari x2",
                "44: Dispari x2",   "45: Rosso x2",
                "46: Nero x2",      "47: Riga1 x3",
                "48: Riga2 x3",     "49: Riga3 x3",
                "50: Cavallo x17",  "51: Quartina x8"
            };
            int nV=16,meta=8;
            int zonaInput=72;
            int spazioLista=PNL_H-ly+(PNL_Y)-zonaInput;
            int maxR=std::min(meta,(int)(spazioLista/lH));
            for(int i=0;i<maxR;i++){
                DrawText(voci[i],   lx0,ly+i*lH,fs,Fade(WHITE,.85f));
                if(i+meta<nV)DrawText(voci[i+meta],lx1,ly+i*lH,fs,Fade(WHITE,.85f));
            }
            int sepY=PNL_Y+PNL_H-zonaInput;
            DrawLine(PNL_X,sepY,PNL_X+PNL_W,sepY,Fade(GOLD,.4f));
            DrawText(g.msg.c_str(), PNL_X+M,sepY+6, 20,ORANGE);
            DrawText(g.buf.c_str(), PNL_X+M,sepY+26,20,YELLOW);
            DrawText("[TAB]=Click  [Z]=Annulla  INVIO vuoto=Gira",PNL_X+M,PNL_Y+PNL_H-16,11,Fade(WHITE,.50f));
        }

        EndDrawing();
    }
    CloseWindow();
    return 0;
}
