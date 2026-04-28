#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXP 64
#define MAXT 10000
#define MAX_PID 32

typedef struct {
    char pid[MAX_PID];
    int arrival;
    int burst;
    int priority;
    int remaining;
    int completion;
    int waiting;
    int turnaround;
    int response;
    int started;
    int finished;
    int lastReadyEnter;
    int starvationFlag;
} Process;

typedef struct {
    char pid[MAX_PID];
    int start;
    int end;
    int core;
    char type[32];
} Segment;

typedef struct {
    int time;
    char ready[512];
    char waiting[512];
    char running[512];
} QueueSnap;

Process p[MAXP], original[MAXP];
Segment segs[MAXT];
QueueSnap snaps[MAXT];
int n = 0, segCount = 0, snapCount = 0;

char algorithm[64] = "fcfs";
int quantum = 2;
int contextSwitch = 0;
int cores = 1;
int agingThreshold = 8;

void trim(char *s) {
    int i, j = 0;
    char temp[1024];
    for (i = 0; s[i]; i++) if (!isspace((unsigned char)s[i])) temp[j++] = s[i];
    temp[j] = 0;
    strcpy(s, temp);
}

int extract_int(const char *json, const char *key, int def) {
    char pattern[64];
    sprintf(pattern, "\"%s\":", key);
    char *pos = strstr(json, pattern);
    if (!pos) return def;
    pos += strlen(pattern);
    return atoi(pos);
}

void extract_str(const char *json, const char *key, char *out, const char *def) {
    char pattern[64];
    sprintf(pattern, "\"%s\":\"", key);
    char *pos = strstr(json, pattern);
    if (!pos) {
        strcpy(out, def);
        return;
    }
    pos += strlen(pattern);
    int i = 0;
    while (*pos && *pos != '"' && i < 63) out[i++] = *pos++;
    out[i] = 0;
}

void load_input(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Cannot open input file\n");
        exit(1);
    }

    char json[20000], line[1024];
    json[0] = 0;
    while (fgets(line, sizeof(line), f)) strcat(json, line);
    fclose(f);

    trim(json);

    extract_str(json, "algorithm", algorithm, "fcfs");
    quantum = extract_int(json, "quantum", 2);
    contextSwitch = extract_int(json, "contextSwitch", 0);
    cores = extract_int(json, "cores", 1);
    agingThreshold = extract_int(json, "agingThreshold", 8);
    if (cores < 1) cores = 1;
    if (cores > 4) cores = 4;

    char *arr = strstr(json, "\"processes\":[");
    if (!arr) return;
    arr = strchr(arr, '[');
    arr++;

    n = 0;
    while (*arr && *arr != ']' && n < MAXP) {
        char *open = strchr(arr, '{');
        if (!open) break;
        char *close = strchr(open, '}');
        if (!close) break;

        char obj[1024];
        int len = close - open + 1;
        strncpy(obj, open, len);
        obj[len] = 0;

        extract_str(obj, "pid", p[n].pid, "P");
        p[n].arrival = extract_int(obj, "arrival", 0);
        p[n].burst = extract_int(obj, "burst", 1);
        p[n].priority = extract_int(obj, "priority", 1);

        p[n].remaining = p[n].burst;
        p[n].completion = 0;
        p[n].waiting = 0;
        p[n].turnaround = 0;
        p[n].response = -1;
        p[n].started = 0;
        p[n].finished = 0;
        p[n].lastReadyEnter = p[n].arrival;
        p[n].starvationFlag = 0;

        n++;
        arr = close + 1;
    }

    for (int i = 0; i < n; i++) original[i] = p[i];
}

void reset_state() {
    segCount = 0;
    snapCount = 0;
    for (int i = 0; i < n; i++) {
        p[i] = original[i];
        p[i].remaining = p[i].burst;
        p[i].completion = 0;
        p[i].waiting = 0;
        p[i].turnaround = 0;
        p[i].response = -1;
        p[i].started = 0;
        p[i].finished = 0;
        p[i].lastReadyEnter = p[i].arrival;
        p[i].starvationFlag = 0;
    }
}

void add_segment(const char *pid, int start, int end, int core, const char *type) {
    if (end <= start) return;
    if (segCount > 0 &&
        strcmp(segs[segCount-1].pid, pid) == 0 &&
        segs[segCount-1].end == start &&
        segs[segCount-1].core == core &&
        strcmp(segs[segCount-1].type, type) == 0) {
        segs[segCount-1].end = end;
        return;
    }
    strcpy(segs[segCount].pid, pid);
    segs[segCount].start = start;
    segs[segCount].end = end;
    segs[segCount].core = core;
    strcpy(segs[segCount].type, type);
    segCount++;
}

int all_done() {
    for (int i = 0; i < n; i++) if (!p[i].finished) return 0;
    return 1;
}

void finalize_metrics() {
    for (int i = 0; i < n; i++) {
        p[i].turnaround = p[i].completion - p[i].arrival;
        p[i].waiting = p[i].turnaround - p[i].burst;
        if (p[i].waiting < 0) p[i].waiting = 0;
    }
}

int select_fcfs(int t) {
    int idx = -1;
    for (int i = 0; i < n; i++) {
        if (!p[i].finished && p[i].arrival <= t && p[i].remaining > 0) {
            if (idx == -1 || p[i].arrival < p[idx].arrival) idx = i;
        }
    }
    return idx;
}

int select_sjf(int t) {
    int idx = -1;
    for (int i = 0; i < n; i++) {
        if (!p[i].finished && p[i].arrival <= t && p[i].remaining > 0) {
            if (idx == -1 || p[i].burst < p[idx].burst) idx = i;
        }
    }
    return idx;
}

int select_priority(int t) {
    int idx = -1;
    for (int i = 0; i < n; i++) {
        if (!p[i].finished && p[i].arrival <= t && p[i].remaining > 0) {
            int effectivePriority = p[i].priority;
            int waited = t - p[i].lastReadyEnter;
            if (waited >= agingThreshold) {
                effectivePriority -= waited / agingThreshold;
                p[i].starvationFlag = 1;
            }
            if (idx == -1) idx = i;
            else {
                int idxEff = p[idx].priority - ((t - p[idx].lastReadyEnter) / agingThreshold);
                if (effectivePriority < idxEff) idx = i;
            }
        }
    }
    return idx;
}

void add_snapshot(int t, int running[], int coreCount) {
    if (snapCount >= MAXT) return;
    snaps[snapCount].time = t;
    snaps[snapCount].ready[0] = 0;
    snaps[snapCount].waiting[0] = 0;
    snaps[snapCount].running[0] = 0;

    for (int i = 0; i < n; i++) {
        int isRun = 0;
        for (int c = 0; c < coreCount; c++) if (running[c] == i) isRun = 1;
        if (!p[i].finished && p[i].arrival <= t && p[i].remaining > 0 && !isRun) {
            strcat(snaps[snapCount].ready, p[i].pid);
            strcat(snaps[snapCount].ready, " ");
        }
    }
    for (int c = 0; c < coreCount; c++) {
        if (running[c] >= 0) {
            strcat(snaps[snapCount].running, p[running[c]].pid);
            strcat(snaps[snapCount].running, " ");
        }
    }
    snaps[snapCount].waiting[0] = 0;
    snapCount++;
}

void run_nonpreemptive(int mode) {
    int t = 0;
    int runningArr[4] = {-1,-1,-1,-1};

    while (!all_done() && t < MAXT) {
        int idx;
        if (mode == 0) idx = select_fcfs(t);
        else if (mode == 1) idx = select_sjf(t);
        else idx = select_priority(t);

        if (idx == -1) {
            add_segment("IDLE", t, t+1, 0, "idle");
            add_snapshot(t, runningArr, 1);
            t++;
            continue;
        }

        if (contextSwitch > 0 && t > 0) {
            add_segment("CS", t, t + contextSwitch, 0, "context");
            t += contextSwitch;
        }

        if (p[idx].response == -1) p[idx].response = t - p[idx].arrival;
        runningArr[0] = idx;
        add_snapshot(t, runningArr, 1);

        int runTime = p[idx].remaining;
        add_segment(p[idx].pid, t, t + runTime, 0, "process");
        t += runTime;
        p[idx].remaining = 0;
        p[idx].finished = 1;
        p[idx].completion = t;
        runningArr[0] = -1;
    }
    finalize_metrics();
}

void run_rr() {
    int t = 0, q[MAXT], front = 0, rear = 0, added[MAXP] = {0};
    int runningArr[4] = {-1,-1,-1,-1};

    while (!all_done() && t < MAXT) {
        for (int i = 0; i < n; i++) {
            if (!added[i] && p[i].arrival <= t) {
                q[rear++] = i;
                added[i] = 1;
                p[i].lastReadyEnter = t;
            }
        }

        if (front == rear) {
            add_segment("IDLE", t, t+1, 0, "idle");
            add_snapshot(t, runningArr, 1);
            t++;
            continue;
        }

        int idx = q[front++];
        if (p[idx].finished) continue;

        if (contextSwitch > 0 && t > 0) {
            add_segment("CS", t, t + contextSwitch, 0, "context");
            t += contextSwitch;
        }

        if (p[idx].response == -1) p[idx].response = t - p[idx].arrival;
        runningArr[0] = idx;
        add_snapshot(t, runningArr, 1);

        int run = p[idx].remaining < quantum ? p[idx].remaining : quantum;
        add_segment(p[idx].pid, t, t + run, 0, "process");

        for (int step = 0; step < run; step++) {
            t++;
            for (int i = 0; i < n; i++) {
                if (!added[i] && p[i].arrival <= t) {
                    q[rear++] = i;
                    added[i] = 1;
                    p[i].lastReadyEnter = t;
                }
            }
        }

        p[idx].remaining -= run;
        runningArr[0] = -1;

        if (p[idx].remaining == 0) {
            p[idx].finished = 1;
            p[idx].completion = t;
        } else {
            p[idx].lastReadyEnter = t;
            q[rear++] = idx;
        }
    }
    finalize_metrics();
}

void write_output(const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return;

    double avgW=0, avgT=0, avgR=0;
    int makespan = 0, busy = 0, starved = 0;
    for (int i=0;i<n;i++) {
        avgW += p[i].waiting;
        avgT += p[i].turnaround;
        avgR += p[i].response;
        if (p[i].completion > makespan) makespan = p[i].completion;
        if (p[i].starvationFlag) starved++;
    }
    for (int i=0;i<segCount;i++) {
        if (strcmp(segs[i].type,"process")==0) busy += segs[i].end - segs[i].start;
    }
    if (n>0) { avgW/=n; avgT/=n; avgR/=n; }

    fprintf(f, "{\n");
    fprintf(f, "\"algorithm\":\"%s\",\n", algorithm);
    fprintf(f, "\"metrics\":{\"averageWaiting\":%.2f,\"averageTurnaround\":%.2f,\"averageResponse\":%.2f,\"cpuUtilization\":%.2f,\"throughput\":%.2f,\"makespan\":%d,\"starvedProcesses\":%d},\n",
        avgW, avgT, avgR, makespan?((busy*100.0)/makespan):0, makespan?((n*1.0)/makespan):0, makespan, starved);

    fprintf(f, "\"processes\":[");
    for (int i=0;i<n;i++) {
        fprintf(f, "{\"pid\":\"%s\",\"arrival\":%d,\"burst\":%d,\"priority\":%d,\"completion\":%d,\"waiting\":%d,\"turnaround\":%d,\"response\":%d,\"starvation\":%s}%s",
            p[i].pid,p[i].arrival,p[i].burst,p[i].priority,p[i].completion,p[i].waiting,p[i].turnaround,p[i].response,p[i].starvationFlag?"true":"false", i==n-1?"":",");
    }
    fprintf(f, "],\n");

    fprintf(f, "\"gantt\":[");
    for (int i=0;i<segCount;i++) {
        fprintf(f, "{\"pid\":\"%s\",\"start\":%d,\"end\":%d,\"core\":%d,\"type\":\"%s\"}%s",
            segs[i].pid,segs[i].start,segs[i].end,segs[i].core,segs[i].type,i==segCount-1?"":",");
    }
    fprintf(f, "],\n");

    fprintf(f, "\"snapshots\":[");
    for (int i=0;i<snapCount;i++) {
        fprintf(f, "{\"time\":%d,\"ready\":\"%s\",\"waiting\":\"%s\",\"running\":\"%s\"}%s",
            snaps[i].time,snaps[i].ready,snaps[i].waiting,snaps[i].running,i==snapCount-1?"":",");
    }
    fprintf(f, "]\n");

    fprintf(f, "}\n");
    fclose(f);
}

void run_selected() {
    reset_state();

    if (strcmp(algorithm,"fcfs")==0) run_nonpreemptive(0);
    else if (strcmp(algorithm,"sjf")==0) run_nonpreemptive(1);
    else if (strcmp(algorithm,"rr")==0) run_rr();
    else if (strcmp(algorithm,"priority")==0) run_nonpreemptive(2);
    else run_nonpreemptive(0);
}

int main(int argc, char **argv) {
    const char *input = "input.json";
    const char *output = "output.json";

    if (argc >= 2) input = argv[1];
    if (argc >= 3) output = argv[2];

    load_input(input);
    run_selected();
    write_output(output);

    return 0;
}
