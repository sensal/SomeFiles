import os
import linecache
import time

PIDS = []       #save all thread id number
PIDS_INFO = [] #save all threads information (thread_ID, thread_name, memory, CPU)

#get all threads id
def get_ID():
    for pid in os.listdir("/proc/"):
        if pid[0] in str(range(10)):
            PIDS.append(pid)

#get every thread used memory then put it into PIDS_INFO
#the thread's memory is not 'ffffffff'
#len(PIDS_INFO) != len(PIDS)
def get_ID_meminfo():
    for pid in PIDS:
        PID_INFO = []
        pid_path = "/proc/" + pid +"/status"
        if linecache.getline(pid_path, 17).split()[1][0] != 'f':
            PID_INFO.append(pid)
            PID_INFO.append(linecache.getline(pid_path, 1).split()[1])
            PID_INFO.append((int)(linecache.getline(pid_path, 17).split()[1])/1024.0)
            PIDS_INFO.append(PID_INFO)

#get the total CPU used information
def get_totalCpu_info():
    with open("/proc/stat") as totalCpu:
        line = totalCpu.readline()
        totalCpuTime = (int)(line.split()[1]) + \
                (int)(line.split()[2]) + \
                (int)(line.split()[3]) + \
                (int)(line.split()[4]) + \
                (int)(line.split()[5]) + \
                (int)(line.split()[6]) + \
                (int)(line.split()[7]) + \
                (int)(line.split()[8]) + \
                (int)(line.split()[9]) + \
                (int)(line.split()[10])
    totalCpu.close()
    return totalCpuTime

#get every thread CPU used information
def get_threadCpu_info():
    threadsCpuTime = []
    for pid in PIDS:
        with open("/proc/"+pid+"/stat") as threadCpu:
            line = threadCpu.readline()
            threadCpuTime = (int)(line.split()[13]) + \
                    (int)(line.split()[14]) + \
                    (int)(line.split()[15]) + \
                    (int)(line.split()[16])
        threadCpu.close()
        threadsCpuTime.append(threadCpuTime)
    return threadsCpuTime

#insert the CPU used information into PIDS_INFO
def insert_to_PIDS_info(args):
    cpuUsed = args
    thread_id_index = 0
    for pid_info in PIDS_INFO:
        PIDS_INFO[thread_id_index].append(cpuUsed[thread_id_index])
        thread_id_index += 1

#get CPU information and insert into PIDS_INFO
def get_ID_cpuinfo():
    totalCpuTime1 = get_totalCpu_info()
    threadCpuTime1 = get_threadCpu_info()

    time.sleep(1)

    totalCpuTime2 = get_totalCpu_info()
    threadCpuTime2 = get_threadCpu_info()

    cpuUsed = []
    for threadTime1, threadTime2 in zip(threadCpuTime1, threadCpuTime2):
        cpuUsed.append((float)(threadTime2 - threadTime1) / (totalCpuTime2 - totalCpuTime1))
    insert_to_PIDS_info(cpuUsed)

#display the information using debug
def display(args):
    for info in args:
        print "%-6s %-16s %-6.2f %-4.2f" % (info[0], info[1], info[2], info[3])

def write_to_file():
    fileHandle = open("information", "w")
    fileHandle.write(str(PIDS_INFO))
    fileHandle.close()

def get_PIDS_INFO():
    return PIDS_INFO

def main():
    get_ID()        #get thread ID
    get_ID_meminfo()#get thread memory information
    get_ID_cpuinfo()#get thread CPU used information

    write_to_file()
    #display(PIDS_INFO)

if __name__ == "__main__":
    main()
