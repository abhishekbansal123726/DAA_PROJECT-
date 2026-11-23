#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PASSENGERS 100
#define MAX_LANES 10
#define INF 1000000000

typedef struct { int id, arrival, priority, duration, order; } Passenger;
typedef struct { int id, lane, start, finish, wait, priority; } Result;
typedef struct { Result rows[MAX_PASSENGERS]; int count, totalWait, finishTime; double avgWait, throughput; int busy[MAX_LANES]; double utilization[MAX_LANES]; } Simulation;

static Passenger passengers[MAX_PASSENGERS];
static int passengerCount = 0, laneCount = 2;

int readInt(const char *prompt, int *out) {
    char buf[128], extra;
    long v;
    printf("%s", prompt);
    if (!fgets(buf, sizeof(buf), stdin)) return 0;
    if (sscanf(buf, " %ld %c", &v, &extra) != 1 || v < -1000000 || v > 1000000) return -1;
    *out = (int)v;
    return 1;
}

void sampleData(void) {
    Passenger sample[] = {{101,0,3,5,0},{102,1,1,3,1},{103,2,2,4,2},
                          {104,3,1,2,3},{105,4,3,3,4},{106,5,2,4,5}};
    passengerCount = 6;
    memcpy(passengers, sample, sizeof(sample));
    printf("Sample data loaded (6 passengers).\n");
}

int cmpArrival(const void *a, const void *b) {
    const Passenger *x = (const Passenger *)a, *y = (const Passenger *)b;
    if (x->arrival != y->arrival) return (x->arrival > y->arrival) - (x->arrival < y->arrival);
    return (x->order > y->order) - (x->order < y->order);
}

void showPassengers(void) {
    int i;
    printf("\nID     Arrival  Priority  Duration\n----------------------------------\n");
    for (i=0;i<passengerCount;i++) printf("%-6d %-8d %-9d %d\n", passengers[i].id,passengers[i].arrival,passengers[i].priority,passengers[i].duration);
}

void enterPassengers(void) {
    Passenger temp[MAX_PASSENGERS];
    int n, i, j, status;
    status = readInt("Number of passengers (1-100): ", &n);
    if (status != 1 || n < 1 || n > MAX_PASSENGERS) { puts("Invalid count; existing data unchanged."); return; }
    for (i=0;i<n;i++) {
        int id,arrival,priority,duration;
        printf("\nPassenger %d\n",i+1);
        if (readInt("  Unique ID: ",&id)!=1 || id<1) goto invalid;
        for (j=0;j<i;j++) if (temp[j].id==id) goto invalid;
        if (readInt("  Arrival (0-100000): ",&arrival)!=1 || arrival<0 || arrival>100000) goto invalid;
        if (readInt("  Priority (1=High, 2=Medium, 3=Low): ",&priority)!=1 || priority<1 || priority>3) goto invalid;
        if (readInt("  Checking duration (1-100000): ",&duration)!=1 || duration<1 || duration>100000) goto invalid;
        temp[i]=(Passenger){id,arrival,priority,duration,i};
    }
    memcpy(passengers,temp,n*sizeof(Passenger));
    passengerCount=n;
    puts("Passenger data saved successfully.");
    return;
invalid:
    puts("Invalid or duplicate passenger data; existing data unchanged.");
}

/* Event-driven FIFO/Priority scheduling with greedy first-free-lane allocation. */
Simulation simulate(int lanes, int usePriority) {
    Simulation s = {0};
    Passenger sorted[MAX_PASSENGERS];
    int freeAt[MAX_LANES]={0}, waiting[MAX_PASSENGERS];
    int next=0, waitingCount=0, done=0, time=0, i;
    memcpy(sorted,passengers,passengerCount*sizeof(Passenger));
    qsort(sorted,passengerCount,sizeof(Passenger),cmpArrival);
    while (done < passengerCount) {
        int lane=-1, pos=0, idx, start, finish, nextTime=INF;
        while (next<passengerCount && sorted[next].arrival<=time) waiting[waitingCount++]=next++;
        for (i=0;i<lanes;i++) if (freeAt[i]<=time) {lane=i;break;}
        if (lane>=0 && waitingCount>0) {
            if (usePriority) for (i=1;i<waitingCount;i++)
                if (sorted[waiting[i]].priority < sorted[waiting[pos]].priority) pos=i;
            idx=waiting[pos];
            for (i=pos;i<waitingCount-1;i++) waiting[i]=waiting[i+1];
            waitingCount--;
            start=time; finish=start+sorted[idx].duration;
            s.rows[s.count++]=(Result){sorted[idx].id,lane+1,start,finish,start-sorted[idx].arrival,sorted[idx].priority};
            s.totalWait+=start-sorted[idx].arrival;
            if (finish>s.finishTime) s.finishTime=finish;
            s.busy[lane]+=sorted[idx].duration;
            freeAt[lane]=finish;
            done++;
            continue;
        }
        if (next<passengerCount && sorted[next].arrival<nextTime) nextTime=sorted[next].arrival;
        for (i=0;i<lanes;i++) if (freeAt[i]>time && freeAt[i]<nextTime) nextTime=freeAt[i];
        if (nextTime==INF) break;
        time=nextTime;
    }
    s.avgWait=s.count ? (double)s.totalWait/s.count : 0;
    s.throughput=s.finishTime ? (double)s.count/s.finishTime : 0;
    for (i=0;i<lanes;i++) s.utilization[i]=s.finishTime ? 100.0*s.busy[i]/s.finishTime : 0;
    return s;
}

void printSimulation(const char *title, const Simulation *s, int lanes) {
    int i;
    printf("\n========== %s ==========\n",title);
    puts("ID     Lane  Start  Finish  Wait  Priority");
    puts("------------------------------------------");
    for (i=0;i<s->count;i++) {
        Result r=s->rows[i];
        printf("P%-5d %-5d %-6d %-7d %-5d %d\n",r.id,r.lane,r.start,r.finish,r.wait,r.priority);
    }
    printf("Passengers processed : %d\nTotal waiting time    : %d min\nAverage waiting time  : %.2f min\nCompletion time       : %d min\nThroughput            : %.3f passengers/min\n",
           s->count,s->totalWait,s->avgWait,s->finishTime,s->throughput);
    for (i=0;i<lanes;i++) printf("Lane %d utilization   : %.2f%%\n",i+1,s->utilization[i]);
}

void jsonRows(FILE *f, const Simulation *s) {
    int i;
    fprintf(f,"{\"averageWait\":%.4f,\"totalWait\":%d,\"completionTime\":%d,\"throughput\":%.6f,\"results\":[",s->avgWait,s->totalWait,s->finishTime,s->throughput);
    for (i=0;i<s->count;i++) {
        Result r=s->rows[i];
        fprintf(f,"%s{\"id\":%d,\"lane\":%d,\"start\":%d,\"finish\":%d,\"wait\":%d,\"priority\":%d}",i?",":"",r.id,r.lane,r.start,r.finish,r.wait,r.priority);
    }
    fprintf(f,"]}");
}

void exportJson(const Simulation *fifo, const Simulation *priority, const Simulation *failure) {
    FILE *f=fopen("simulation_results.json","w");
    int i;
    if (!f) { puts("WARNING: Could not write simulation_results.json"); return; }
    fprintf(f,"{\n\"laneCount\":%d,\"passengers\":[",laneCount);
    for (i=0;i<passengerCount;i++) {
        Passenger p=passengers[i];
        fprintf(f,"%s{\"id\":%d,\"arrival\":%d,\"priority\":%d,\"duration\":%d}",i?",":"",p.id,p.arrival,p.priority,p.duration);
    }
    fprintf(f,"],\n\"fifo\":");jsonRows(f,fifo);
    fprintf(f,",\n\"priority\":");jsonRows(f,priority);
    fprintf(f,",\n\"laneFailure\":");jsonRows(f,failure);
    fprintf(f,"\n}\n");
    if (fclose(f)==0) puts("\nGUI data exported: simulation_results.json");
    else puts("WARNING: Error saving JSON file.");
}

void demo(void) {
    Simulation fifo,priority,failure;
    if (!passengerCount) { puts("No passengers loaded."); return; }
    fifo=simulate(laneCount,0);
    priority=simulate(laneCount,1);
    failure=simulate(laneCount-1,0);
    printSimulation("FIFO SCHEDULING",&fifo,laneCount);
    printSimulation("PRIORITY SCHEDULING",&priority,laneCount);
    printSimulation("LANE FAILURE (ONE LANE UNAVAILABLE, FIFO)",&failure,laneCount-1);
    puts("\n========== COMPARISON ==========");
    printf("FIFO average wait          : %.2f min\nPriority average wait      : %.2f min\nLane failure average wait  : %.2f min\nFailure impact vs FIFO     : %+.2f min\n",
           fifo.avgWait,priority.avgWait,failure.avgWait,failure.avgWait-fifo.avgWait);
    if (fifo.avgWait<priority.avgWait) puts("FIFO has lower average waiting time for this input.");
    else if (priority.avgWait<fifo.avgWait) puts("Priority has lower average waiting time for this input.");
    else puts("FIFO and Priority have equal average waiting time for this input.");
    exportJson(&fifo,&priority,&failure);
}

int main(void) {
    int choice, lanes;
    sampleData();
    for (;;) {
        puts("\n==============================================");
        puts(" AIRPORT SECURITY SIMULATOR & RESOURCE ALLOCATION");
        puts("==============================================");
        printf("Passengers: %d | Normal security lanes: %d\n",passengerCount,laneCount);
        puts("1. Show passenger data");
        puts("2. Enter passenger data");
        puts("3. Reset sample data");
        puts("4. Set normal lane count (2-10)");
        puts("5. Run complete demo and export GUI results");
        puts("0. Exit");
        if (readInt("Choose an option: ",&choice)!=1) { puts("Input closed or invalid. Exiting."); break; }
        switch (choice) {
            case 1: showPassengers(); break;
            case 2: enterPassengers(); break;
            case 3: sampleData(); break;
            case 4:
                if (readInt("Number of lanes (2-10): ",&lanes)==1 && lanes>=2 && lanes<=MAX_LANES) {laneCount=lanes;puts("Lane count updated.");}
                else puts("Invalid lane count.");
                break;
            case 5: demo(); break;
            case 0: puts("Goodbye!"); return 0;
            default: puts("Invalid menu option.");
        }
    }
    return 0;
}
