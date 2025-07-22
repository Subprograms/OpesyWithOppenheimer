#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include <vector>
#include <deque>
#include <fstream>
#include <sstream>
#include <mutex>
#include <string>
#include <cstdint>

struct PageTableEntry { bool present; int frame; bool dirty; bool ref; };

class MemoryManager {
public:
    MemoryManager(int maxMem, int frameSz)
        : frameSize(frameSz),
          numFrames(maxMem / frameSz),
          storeFile("csopesy-backing-store.txt"),
          nPagesPagedIn(0),
          nPagesPagedOut(0)
    {
        frames.assign(numFrames, std::vector<uint16_t>(frameSize / 2, 0));
        for (int i = 0; i < numFrames; ++i) freeList.push_back(i);
        std::ofstream reset(storeFile, std::ios::trunc);
        reset << "page:w0 w1 w2 w3 w4 w5 w6 w7\n";
    }

    bool access(int, uint32_t addr, bool write, uint16_t& val)
    {
        std::lock_guard<std::mutex> lk(mtx);
        int pg = addr / frameSize;
        if (pg >= static_cast<int>(pageTable.size()))
            pageTable.resize(pg + 1, {false, -1, false, false});
        auto& ent = pageTable[pg];
        if (!ent.present) pageFault(pg);
        int f = ent.frame;
        int idx = (addr % frameSize) / 2;
        ent.ref = true;
        if (write) {
            if (val > 65535) val = 65535;
            frames[f][idx] = val;
            ent.dirty = true;
            writePage(pg, f);
        } else {
            val = frames[f][idx];
        }
        return true;
    }

    size_t pagesPagedIn()  const { return nPagesPagedIn; }
    size_t pagesPagedOut() const { return nPagesPagedOut; }

private:
    int frameSize, numFrames;
    std::vector<std::vector<uint16_t>> frames;
    std::vector<PageTableEntry> pageTable;
    std::deque<int> freeList, fifoQueue;
    std::string storeFile;
    std::mutex mtx;
    size_t nPagesPagedIn, nPagesPagedOut;

    void pageFault(int pg)
    {
        if (freeList.empty()) evict();
        int f = freeList.front(); freeList.pop_front();
        fifoQueue.push_back(f);
        pageTable[pg] = {true, f, false, true};
        loadPage(pg, f);
        ++nPagesPagedIn;
    }

    void evict()
    {
        while (true)
        {
            int f = fifoQueue.front(); fifoQueue.pop_front();
            for (auto& e : pageTable)
                if (e.present && e.frame == f) {
                    if (e.ref) { e.ref = false; fifoQueue.push_back(f); }
                    else {
                        if (e.dirty) writePage(static_cast<int>(&e - &pageTable[0]), f);
                        e.present = false; e.frame = -1; e.dirty = false;
                        freeList.push_back(f);
                        return;
                    }
                    break;
                }
        }
    }

    void loadPage(int pg, int f)
    {
        std::ifstream in(storeFile);
        std::string line;
        std::getline(in, line);
        const std::string tag = std::to_string(pg) + ':';
        while (std::getline(in, line))
            if (line.rfind(tag, 0) == 0) {
                std::istringstream iss(line.substr(tag.size()));
                for (auto& w : frames[f]) iss >> w;
                return;
            }
        std::fill(frames[f].begin(), frames[f].end(), 0);
    }

    void writePage(int pg, int frameIdx)
    {
        std::ostringstream newline;
        newline << pg << ':';
        for (auto w : frames[frameIdx]) newline << w << ' ';
        newline << '\n';
        std::ifstream in(storeFile);
        std::stringstream buf;
        std::string line;
        bool replaced = false;
        std::getline(in, line);
        buf << line << '\n';
        const std::string tag = std::to_string(pg) + ':';
        while (std::getline(in, line)) {
            if (line.rfind(tag, 0) == 0) { buf << newline.str(); replaced = true; }
            else buf << line << '\n';
        }
        if (!replaced) buf << newline.str();
        std::ofstream out(storeFile, std::ios::trunc);
        out << buf.str();
        ++nPagesPagedOut;
    }
};

#endif