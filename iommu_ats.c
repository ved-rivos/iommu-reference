typedef struct {
    uint8_t  free;
    uint8_t  DSV;
    uint8_t  DSEG;
    uint16_t RID;
    uint32_t timer;
    uint8_t  num_resp_rcvd;
} itag_tracker_t;

#define MAX_ITAGS 32

itag_tracker_t itag_tracker[MAX_ITAGS];
uint8_t
allocate_itag(DSV, DSEG, RID, &itag) { 
    uint8_t i;
    for ( i = 0; i < MAX_ITAGS; i++ )
        if ( itag_tracker[i].free == 1 ) break;

    if ( i == MAX_ITAGS ) return 1;

    itag_tracker[i].free = 0;
    itag_tracker[i].DSV = DSV;
    itag_tracker[i].DSEC = DSEG;
    itag_tracker[i].RID = RID;
    itag_tracker[i].num_rsp_rcvd = 0;
    itag_tracker[i].timer = 32;
    *itag = i;
    return 0;
}

