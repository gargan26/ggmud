/*  GGMud
 *  Copyright (C) 1999 Heathen (the.heathen@swipnet.se)
 *                1999 Drizzt  (doc.day@swipnet.se)
 *                2003 Gabry (gabrielegreco@gmail.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/* Do all the telnet protocol stuff */

//#include <unistd.h>
#include "config.h"
#include "ggmud.h"

//extern void tintin_printf(struct session *ses, char *format, ...);
//extern void tintin_eprintf(struct session *ses, char *format, ...);

// #define TELNET_DEBUG

#ifdef TELNET_SUPPORT

#define tintin_printf(a, b, args...) if (debugfile) fprintf(debugfile, b, ## args )
#define tintin_eprintf(a, b, args...) if (debugfile) fprintf(debugfile, b, ## args )

extern int LINES,COLS,isstatus;
extern struct session *sessionlist;
extern void pty_resize(int fd,int sx,int sy);
extern void syserr(char *msg, ...);

#ifdef TELNET_DEBUG
static FILE *debugfile = NULL;
#endif

#define EOR 239     /* End of Record */
#define SE  240     /* subnegotiation end */
#define NOP 241     /* no operation */
#define DM  242     /* Data Mark */
#define BRK 243     /* Break */
#define IP  244     /* interrupt process */
#define AO  245     /* abort output */
#define AYT 246     /* Are you there? */
#define EC  247     /* erase character */
#define EL  248     /* erase line */
#define GA  249     /* go ahead */
#define SB  250     /* subnegotiations */
#define WILL    251
#define WONT    252
#define DO      253
#define DONT    254
#define IAC 255     /* interpret as command */

#define ECHO                1
#define SUPPRESS_GO_AHEAD   3
#define STATUS              5
#define TERMINAL_TYPE       24
#define END_OF_RECORD       25
#define NAWS                31
#define MSP                 90

#define IS      0
#define SEND    1

/* sanity check */
#define MAX_SUBNEGO_LENGTH 64

#ifdef TELNET_DEBUG
char *will_names[4]={"WILL", "WONT", "DO", "DONT"};
char *option_names[]=
    {
        "Binary Transmission",
        "Echo",
        "Reconnection",
        "Suppress Go Ahead",
        "Approx Message Size Negotiation",
        "Status",
        "Timing Mark",
        "Remote Controlled Trans and Echo",
        "Output Line Width",
        "Output Page Size",
        "Output Carriage-Return Disposition",
        "Output Horizontal Tab Stops",
        "Output Horizontal Tab Disposition",
        "Output Formfeed Disposition",
        "Output Vertical Tabstops",
        "Output Vertical Tab Disposition",
        "Output Linefeed Disposition",
        "Extended ASCII",
        "Logout",
        "Byte Macro",
        "Data Entry Terminal",
        "SUPDUP",
        "SUPDUP Output",
        "Send Location",
        "Terminal Type",
        "End of Record",
        "TACACS User Identification",
        "Output Marking",
        "Terminal Location Number",
        "Telnet 3270 Regime",
        "X.3 PAD",
        "Negotiate About Window Size",
        "Terminal Speed",
        "Remote Flow Control",
        "Linemode",
        "X Display Location",
        "Environment Option",
        "Authentication Option",
        "Encryption Option",
        "New Environment Option",
    };
#endif

void telnet_send_naws(struct session *ses)
{
#ifdef UI_FULLSCREEN
    unsigned char nego[128],*np;

#define PUTBYTE(b)    if ((b)==255) *np++=255;   *np++=(b);
    np=nego;
    *np++=IAC;
    *np++=SB;
    *np++=NAWS;
    PUTBYTE(COLS/256);
    PUTBYTE(COLS%256);
    PUTBYTE((LINES-1-!!isstatus)/256);
    PUTBYTE((LINES-1-!!isstatus)%256);
    *np++=IAC;
    *np++=SE;
    send(ses->socket, nego, np-nego, 0);
#ifdef TELNET_DEBUG
    {
        char buf[BUFFER_SIZE],*b=buf;
        int neb=np-nego-2;
        np=nego+3;
        b=buf+sprintf(buf, "IAC SB NAWS ");
        while (np-nego<neb)
            b+=sprintf(b, "<%u> ", *np++);
        b+=sprintf(b, "IAC SE");
        tintin_printf(ses, "~8~[telnet] sent: %s~-1~", buf);
    }
#endif
#endif
/* no UI_FULLSCREEN -> no NAWS */
}

void telnet_send_ttype(struct session *ses)
{
    static int last_term_type = 0;
    
    char nego[128],*ttype;

    switch(last_term_type++)
    {
    case 0:
        ttype="ggmud";
        break;
    case 1:
        ttype="hardcopy";
        break;
    case 2:
        ttype="unknown";
        break;
    /* contrary to what zMud does, we cannot claim we're "vt100" or "ansi", */
    /* as we obviously lack an addressable cursor */
    default:
        last_term_type=0;
    case 3:
        ttype="ggmud-"VERSION;
    }
    send(ses->socket, nego,
        sprintf(nego, "%c%c%c%c%s%c%c", IAC, SB,
            TERMINAL_TYPE, IS, ttype, IAC, SE), 0);
#ifdef TELNET_DEBUG
    tintin_printf(ses, "~8~[telnet] sent: IAC SB TERMINAL-TYPE IS \"%s\" IAC SE~-1~", ttype);
#endif
}

#ifdef UI_FULLSCREEN
void telnet_resize_all(void)
{
    struct session *sp;

    for (sp=sessionlist; sp; sp=sp->next)
        if (sp->naws)
        {
            if (sp->issocket)
                telnet_send_naws(sp);
            else
                pty_resize(sp->socket,COLS,LINES-1-!!isstatus);
        }
}
#endif

#define TELOPT_COMPRESS 85
#define TELOPT_COMPRESS2 86

int do_telnet_protocol(unsigned char *data,int nb,struct session *ses)
{
    unsigned char *cp = data+1;
    unsigned char wt;
    unsigned char answer[3];
    unsigned char nego[128],*np;

#ifdef TELNET_DEBUG
    if (!debugfile)
        debugfile = fopen("debug.txt", "w");
#endif
    
#define NEXTCH  cp++;               \
                if (cp-data>=nb)    \
                    return -1;

    if (nb<2)
        return -1;
    switch(*cp)
    {
    case WILL:
    case WONT:
    case DO:
    case DONT:

        if (nb<3)
            return -1;
        wt=*(cp++);
#ifdef TELNET_DEBUG
        tintin_printf(ses, "~8~[telnet] received: IAC %s <%u> (%s)~-1~",
                      will_names[wt-251], *cp, option_names[*cp]);
#endif
        answer[0]=IAC;
        answer[2]=*cp;

        switch(*cp)
        {
            case MSP:
                fprintf(stderr, "MSP negotiation: %d\n", wt);
                switch(wt)
                {
                    case DO:    answer[1]=WONT; break;
                    case WILL:  
                                if (prefs.UseMSP) {
                                    answer[1]=DO; 
                                    if (!mud->msp)
                                        mud->msp = init_msp(); 
                                }
                                else 
                                    answer[1]=DONT;
                                break;
                    case WONT:  
                                answer[1]=DONT; 
                                if (mud->msp) {
                                        free(mud->msp);
                                        mud->msp = NULL;
                                }
                                break;
                    case DONT:  answer[1]=WONT; break;
                };
                break;
            case ECHO:
                switch(wt)
                {
                    case DO:    answer[1]=WONT; break;
                    case WILL:  answer[1]=DO; mud->ent->visible=0; input_line_visible(FALSE); break;
                    case WONT:  answer[1]=DONT; if (!mud->ent->visible) { mud->ent->visible=1; input_line_visible(TRUE); } break;
                    case DONT:  answer[1]=WONT; break;
                };
                break;
            case TERMINAL_TYPE:
                switch(wt)
                {
                    case WILL:  answer[1]=DONT; break;
                    case DO:    answer[1]=WILL; break;
                    case WONT:  answer[1]=DONT; break;
                    case DONT:  answer[1]=WONT; break;
                };
                break;
#ifdef UI_FULLSCREEN    
            case NAWS:
                switch(wt)
                {
                    case WILL:  answer[1]=DO;   ses->naws=1; break;
                    case DO:    answer[1]=WILL; ses->naws=1; break;
                    case WONT:  answer[1]=DONT; ses->naws=0; break;
                    case DONT:  answer[1]=WONT; ses->naws=0; break;
                };
                break;
#endif
            case END_OF_RECORD:
                switch(wt)
                {
                    case WILL:  answer[1]=DO;   break;
                    case DO:    answer[1]=WONT; break;
                    case WONT:  answer[1]=DONT; break;
                    case DONT:  answer[1]=WONT; break;
                };
                break;
            default:
                switch(wt)
                {
                    case WILL:  answer[1]=DONT; break;
                    case DO:    answer[1]=WONT; break;
                    case WONT:  answer[1]=DONT; break;
                    case DONT:  answer[1]=WONT; break;
                };
        }
        send(ses->socket, answer, 3, 0);
#ifdef TELNET_DEBUG
        tintin_printf(ses, "~8~[telnet] sent: IAC %s <%u> (%s)~-1~",
                      will_names[answer[1]-251], *cp, option_names[*cp]);
#endif
        if (*cp==NAWS)
            telnet_send_naws(ses);
        return 3;
    case SB:
        np=nego;
sbloop:
        NEXTCH;
        while (*cp!=IAC)
        {
            if (np-nego>MAX_SUBNEGO_LENGTH)
                goto nego_too_long;
            *np++=*cp;
            NEXTCH;
        }
        NEXTCH;
        if (*cp==IAC)
        {
            if (np-nego>MAX_SUBNEGO_LENGTH)
                goto nego_too_long;
            *np++=IAC;
            goto sbloop;
        }
        if (*cp!=SE)
        {
            if (np-nego>MAX_SUBNEGO_LENGTH)
                goto nego_too_long;
            *np++=*cp;
            goto sbloop;
        }
        nb=cp-data;
#ifdef TELNET_DEBUG
        {
            char buf[BUFFER_SIZE],*b=buf;
            unsigned int neb=np-nego;
            np=nego;
            b=buf+sprintf(buf, "IAC SB ");
            switch(*np)
            {
            case TERMINAL_TYPE:
                b+=sprintf(b, "TERMINAL-TYPE ");
                if (*++np==SEND)
                {
                    b+=sprintf(b, "SEND ");
                    ++np;
                }
                break;
            }
            while (np-nego<neb)
                b+=sprintf(b, "<%u> ", *np++);
            b+=sprintf(b, "IAC SE");
            tintin_printf(ses, "~8~[telnet] received: %s~-1~", buf);
        }
#endif
        switch(*(np=nego))
        {
        case TERMINAL_TYPE:
            if (*(np+1)==SEND)
                telnet_send_ttype(ses);
            break;
        }
        return nb+1;
    case GA:
    case EOR:
#ifdef TELNET_DEBUG
        tintin_printf(ses, "~8~[telnet] received: IAC %s~-1~",
            (*cp==GA)?"GA":"EOR");
#endif
//        ses->gas=1;
        return -2;
    case IAC:       /* IAC IAC is the escape for literal 255 byte */
        return -3;
    default:
        /* other 2-byte command, ignore */
#ifdef TELNET_DEBUG
        tintin_printf(ses, "~8~[telnet] received: IAC <%u>~-1~", *cp&255);
#endif
        return 2;
    }
    return (cp-data);
nego_too_long:
#ifdef TELNET_DEBUG
    tintin_eprintf(ses, "#error: unterminated TELNET subnegotiation received.");
#endif
    return 2; /* we leave everything but IAC SB */
}

void telnet_write_line(char *line, struct session *ses)
{
    char outtext[2*BUFFER_SIZE + 2],*out;

    out=outtext;
    while(*line)
    {
        if ((unsigned char)*line==255)
            *out++=255;
        *out++=*line++;
    }
    *out++='\r';
    *out++='\n';
    *out=0;

    if (send(ses->socket, outtext, out-outtext, 0) == -1)
        syserr("write in telnet_write_line()");
}

#endif
