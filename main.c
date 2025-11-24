#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ORDEM 3
#define MAX_KEYS (ORDEM - 1)
#define MAX_CHILD ORDEM

typedef struct {
    int n;
    char key[MAX_KEYS][64];
    long pos[MAX_KEYS];    
    long child[MAX_CHILD];
    int folha;
} Pagina;

Pagina raiz;
FILE *idx;
FILE *dat;

typedef struct {
    int w,h,max;
    unsigned char *p;
} PGM;

PGM lerPGM(const char *path){
    PGM img;
    FILE *f=fopen(path,"rb"); 
    if(!f){ printf("Erro abrir\n"); exit(1); }

    char m[4]; fscanf(f,"%s",m);
    fscanf(f,"%d%d%d",&img.w,&img.h,&img.max);
    fgetc(f);
    img.p = malloc(img.w*img.h);
    fread(img.p,1,img.w*img.h,f);
    fclose(f);
    return img;
}

void salvarBinaria(PGM img, int L, long *posOut){
    unsigned char *b = malloc(img.w*img.h);
    for(int i=0;i<img.w*img.h;i++) b[i] = img.p[i] > L ? 255 : 0;

    fseek(dat,0,SEEK_END);
    *posOut = ftell(dat);

    fwrite(&img.w,sizeof(int),1,dat);
    fwrite(&img.h,sizeof(int),1,dat);
    fwrite(b,1,img.w*img.h,dat);

    free(b);
}

long salvarPagina(Pagina *p){
    fseek(idx,0,SEEK_END);
    long pos = ftell(idx);
    fwrite(p,sizeof(Pagina),1,idx);
    return pos;
}

void carregarPagina(Pagina *p,long pos){
    fseek(idx,pos,SEEK_SET);
    fread(p,sizeof(Pagina),1,idx);
}

long splitPagina(Pagina *p, char *promoK, long *promoPos){
    Pagina nova={0};
    int meio=p->n/2;

    strcpy(promoK, p->key[meio]);
    *promoPos = p->pos[meio];

    for(int i=meio+1,j=0;i<p->n;i++,j++){
        strcpy(nova.key[j], p->key[i]);
        nova.pos[j]=p->pos[i];
        nova.child[j]=p->child[i];
        nova.n++;
    }
    nova.child[nova.n] = p->child[p->n];
    nova.folha=p->folha;

    p->n = meio;

    long posOld = salvarPagina(p);
    long posNova= salvarPagina(&nova);

    return posNova;
}

long inserirRec(long ref, char *k, long posImg,
                char *promoK,long *promoPos){
    if(ref==-1){
        Pagina p={0};
        p.folha=1;
        p.n=1;
        strcpy(p.key[0],k);
        p.pos[0]=posImg;
        return salvarPagina(&p);
    }

    Pagina p;
    carregarPagina(&p,ref);

    if(p.folha){
        int i=p.n-1;
        while(i>=0 && strcmp(k,p.key[i])<0){
            strcpy(p.key[i+1],p.key[i]);
            p.pos[i+1]=p.pos[i];
            i--;
        }
        strcpy(p.key[i+1],k);
        p.pos[i+1]=posImg;
        p.n++;

        if(p.n < MAX_KEYS){
            salvarPagina(&p);
            return -1;
        }

        long npos = splitPagina(&p,promoK,promoPos);
        return npos;
    }

    int i=p.n-1;
    while(i>=0 && strcmp(k,p.key[i])<0) i--;
    i++;

    char pk[64];
    long pp;
    long ret = inserirRec(p.child[i],k,posImg,pk,&pp);

    if(ret==-1){
        salvarPagina(&p);
        return -1;
    }

    int j=p.n-1;
    while(j>=0 && strcmp(pk,p.key[j])<0){
        strcpy(p.key[j+1],p.key[j]);
        p.pos[j+1]=p.pos[j];
        p.child[j+2]=p.child[j+1];
        j--;
    }
    strcpy(p.key[j+1],pk);
    p.pos[j+1]=pp;
    p.child[j+2]=ret;
    p.n++;

    if(p.n < MAX_KEYS){
        salvarPagina(&p);
        return -1;
    }

    long npos = splitPagina(&p,promoK,promoPos);
    return npos;
}

void inserir(char *k,long posImg){
    char promoK[64];
    long promoPos;

    long ret = inserirRec(-1,k,posImg,promoK,&promoPos);

    if(ret!=-1){
        Pagina nova={0};
        strcpy(nova.key[0],promoK);
        nova.pos[0]=promoPos;
        nova.child[0]=salvarPagina(&raiz);
        nova.child[1]=ret;
        nova.n=1;
        nova.folha=0;
        raiz=nova;
    }
}

// ----------- REMOÇÃO (versão completa porém compacta) -------------
int buscarChave(Pagina *p, char *k){
    int i=0;
    while(i<p->n && strcmp(k,p->key[i])>0) i++;
    return i;
}

// remover da folha
void removerDeFolha(Pagina *p, int idx){
    for(int i=idx;i<p->n-1;i++){
        strcpy(p->key[i],p->key[i+1]);
        p->pos[i]=p->pos[i+1];
    }
    p->n--;
}

// maior chave da subárvore esquerda
void getPredecessor(long ref, char *kOut, long *posOut){
    Pagina p; carregarPagina(&p,ref);
    while(!p.folha){
        long ch = p.child[p.n];
        carregarPagina(&p,ch);
    }
    strcpy(kOut,p.key[p.n-1]);
    *posOut=p.pos[p.n-1];
}

// menor chave da subárvore direita
void getSucessor(long ref, char *kOut,long *posOut){
    Pagina p; carregarPagina(&p,ref);
    while(!p.folha){
        long ch = p.child[0];
        carregarPagina(&p,ch);
    }
    strcpy(kOut,p.key[0]);
    *posOut=p.pos[0];
}

void mergePaginas(Pagina *p,int idx){
    int i;

    // filho esq = idx
    Pagina e,d;
    carregarPagina(&e,p->child[idx]);
    carregarPagina(&d,p->child[idx+1]);

    strcpy(e.key[e.n], p->key[idx]);
    e.pos[e.n] = p->pos[idx];
    e.n++;

    for(i=0;i<d.n;i++){
        strcpy(e.key[e.n+i], d.key[i]);
        e.pos[e.n+i]=d.pos[i];
    }
    for(i=0;i<=d.n;i++)
        e.child[e.n+i]=d.child[i];

    e.n += d.n;

    long posE = salvarPagina(&e);

    for(i=idx;i<p->n-1;i++){
        strcpy(p->key[i],p->key[i+1]);
        p->pos[i]=p->pos[i+1];
        p->child[i+1]=p->child[i+2];
    }
    p->n--;
    p->child[idx]=posE;
}

void removerRec(long ref,char *k){
    Pagina p;
    carregarPagina(&p,ref);

    int idx = buscarChave(&p,k);

    // caso 1: chave está nesta página
    if(idx < p.n && strcmp(p.key[idx],k)==0){

        if(p.folha){
            removerDeFolha(&p,idx);
            salvarPagina(&p);
            return;
        }

        // não folha → predecessor ou sucessor
        char kPred[64];
        long posPred;
        getPredecessor(p.child[idx],kPred,&posPred);
        strcpy(p.key[idx],kPred);
        p.pos[idx]=posPred;

        salvarPagina(&p);
        removerRec(p.child[idx], kPred);
        return;
    }

    // caso 2: chave não está aqui
    if(p.folha) return;

    int flag = (idx==p.n);

    Pagina filho;
    carregarPagina(&filho,p.child[idx]);

    if(filho.n == 1){
        // tentativa de redistribuição à esquerda
        if(idx>0){
            Pagina esq;
            carregarPagina(&esq,p.child[idx-1]);
            if(esq.n > 1){
                // rotacionar
                for(int i=filho.n;i>0;i--){
                    strcpy(filho.key[i],filho.key[i-1]);
                    filho.pos[i]=filho.pos[i-1];
                }
                for(int i=filho.n+1;i>0;i--)
                    filho.child[i]=filho.child[i-1];

                strcpy(filho.key[0],p.key[idx-1]);
                filho.pos[0]=p.pos[idx-1];

                filho.child[0]=esq.child[esq.n];

                strcpy(p.key[idx-1], esq.key[esq.n-1]);
                p.pos[idx-1]=esq.pos[esq.n-1];

                esq.n--;

                salvarPagina(&esq);
                salvarPagina(&filho);
                salvarPagina(&p);

            } else {
                mergePaginas(&p,idx-1);
                salvarPagina(&p);
                removerRec(p.child[idx-1],k);
                return;
            }
        }
        // redistribuição direita
        else {
            Pagina dir;
            carregarPagina(&dir,p.child[idx+1]);
            if(dir.n > 1){
                strcpy(filho.key[filho.n],p.key[idx]);
                filho.pos[filho.n] = p.pos[idx];
                filho.child[filho.n+1]=dir.child[0];
                filho.n++;

                strcpy(p.key[idx], dir.key[0]);
                p.pos[idx]=dir.pos[0];

                for(int i=0;i<dir.n-1;i++){
                    strcpy(dir.key[i],dir.key[i+1]);
                    dir.pos[i]=dir.pos[i+1];
                    dir.child[i]=dir.child[i+1];
                }
                dir.child[dir.n-1]=dir.child[dir.n];
                dir.n--;

                salvarPagina(&filho);
                salvarPagina(&dir);
                salvarPagina(&p);
            } else {
                mergePaginas(&p,idx);
                salvarPagina(&p);
                removerRec(p.child[idx],k);
                return;
            }
        }
    }

    long next = (flag ? p.child[idx-1] : p.child[idx]);
    removerRec(next,k);
}

void remover(char *k){
    removerRec(salvarPagina(&raiz),k);

    Pagina r;
    carregarPagina(&r,salvarPagina(&raiz));

    if(!r.folha && r.n==0){
        carregarPagina(&raiz,r.child[0]);
    }
}

// ---------- PERCURSO ORDENADO (para compactação) -----------
typedef struct {
    char key[64];
    long posAntigo;
} Registro;

Registro lista[1000];
int qtdLista = 0;

void percursoLista(long ref){
    if(ref==-1) return;
    Pagina p; carregarPagina(&p,ref);
    for(int i=0;i<p.n;i++){
        percursoLista(p.child[i]);
        strcpy(lista[qtdLista].key,p.key[i]);
        lista[qtdLista].posAntigo=p.pos[i];
        qtdLista++;
    }
    percursoLista(p.child[p.n]);
}

// -------- COMPACTAÇÃO DO ARQUIVO DE DADOS ----------
void compactar(){
    qtdLista=0;
    long rpos=salvarPagina(&raiz);
    percursoLista(rpos);

    FILE *novo = fopen("dados_new.bin","wb");

    for(int i=0;i<qtdLista;i++){
        fseek(dat, lista[i].posAntigo, SEEK_SET);

        int w,h;
        fread(&w,sizeof(int),1,dat);
        fread(&h,sizeof(int),1,dat);

        unsigned char *buf = malloc(w*h);
        fread(buf,1,w*h,dat);

        long novaPos = ftell(novo);
        fwrite(&w,sizeof(int),1,novo);
        fwrite(&h,sizeof(int),1,novo);
        fwrite(buf,1,w*h,novo);

        free(buf);
    }

    fclose(dat);
    fclose(novo);
    remove("dados.bin");
    rename("dados_new.bin","dados.bin");

    dat = fopen("dados.bin","r+b");
}

// ================= MENU ==================
int main(){
    idx=fopen("indice.bin","w+b");
    dat=fopen("dados.bin","w+b");

    raiz.n=0;
    raiz.folha=1;

    int op;
    char path[128];

    do{
        printf("\n1-Inserir\n2-Listar\n3-Remover\n4-Compactar\n5-Sair\n> ");
        scanf("%d",&op);

        if(op==1){
            printf("Caminho PGM: ");
            scanf("%s",path);

            PGM img = lerPGM(path);

            int qtd;
            printf("Qtd limiares: ");
            scanf("%d",&qtd);

            for(int i=0;i<qtd;i++){
                int L;
                printf("Limiar %d: ",i+1);
                scanf("%d",&L);
                long pos;
                salvarBinaria(img,L,&pos);

                char chave[64];
                sprintf(chave,"%s_%03d",path,L);
                inserir(chave,pos);
            }
            free(img.p);
        }

        else if(op==2){
            qtdLista=0;
            long rpos = salvarPagina(&raiz);
            percursoLista(rpos);
            for(int i=0;i<qtdLista;i++)
                printf("%s\n", lista[i].key);
        }

        else if(op==3){
            char k[64];
            printf("Chave remover: ");
            scanf("%s",k);
            remover(k);
        }

        else if(op==4){
            compactar();
            printf("Compactado.\n");
        }

    }while(op!=5);

    fclose(idx);
    fclose(dat);
}
