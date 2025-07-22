#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include <vector>
#include <unordered_map>
#include <deque>
#include <fstream>
#include <sstream>
#include <mutex>
#include <string>
#include <cstdint>

struct PageTableEntry { bool present; int frame; bool dirty; };

class MemoryManager {
public:
    MemoryManager(int maxMem, int frameSz)
        : frameSize(frameSz),
          numFrames(maxMem / frameSz),
          storeFile("csopesy-backing-store.txt"),
          nPagesPagedIn(0),
          nPagesPagedOut(0)
    {
        // physical frames (each frame holds frameSize bytes / 2 bytes per word)
        frames.assign(numFrames, std::vector<uint16_t>(frameSize / 2, 0));
        for (int i = 0; i < numFrames; ++i) freeList.push_back(i);

        // reset backing store on every run and add an easy‑to‑read header line
        std::ofstream reset(storeFile, std::ios::trunc);
        reset << "page:w0 w1 w2 w3 w4 w5 w6 w7\n"; // w = words (16 bits)
    }

    bool access(int /*pid*/, uint32_t addr, bool write, uint16_t& val)
    {
        std::lock_guard<std::mutex> lk(mtx);

        int pg = addr / frameSize;
        if (pg >= static_cast<int>(pageTable.size()))
            pageTable.resize(pg + 1, {false, -1, false});

        auto& ent = pageTable[pg];
        if (!ent.present) pageFault(pg); // demand paging

        int f   = ent.frame;
        int idx = (addr % frameSize) / 2; // word index inside frame

        if (write) {
            if (val > 65535) val = 65535; // clamp to uint16 range
            frames[f][idx] = val;
            ent.dirty      = true; // mark page dirty
            writePage(pg, f);      // flush immediately for clarity
        } else {
            val = frames[f][idx];
        }
        return true;
    }

    size_t pagesPagedIn()  const { return nPagesPagedIn; }
    size_t pagesPagedOut() const { return nPagesPagedOut; }

private:
    int frameSize, numFrames;
    std::vector<std::vector<uint16_t>> frames; // physical memory
    std::vector<PageTableEntry> pageTable;     // global page table
    std::deque<int> freeList, fifoQueue;       // free frames and FIFO queue
    std::string storeFile;
    std::mutex mtx;

    size_t nPagesPagedIn;
    size_t nPagesPagedOut;

    // handle a page fault: get a free frame, load the page, update page table
    void pageFault(int pg)
    {
        if (freeList.empty()) evict(); // run replacement if full
        int f = freeList.front(); freeList.pop_front();
        fifoQueue.push_back(f);
        pageTable[pg] = {true, f, false};
        loadPage(pg, f);
        ++nPagesPagedIn;
    }

    // evict the oldest frame using FIFO
    void evict()
    {
        int f = fifoQueue.front(); fifoQueue.pop_front();
        for (auto& e : pageTable)
            if (e.present && e.frame == f) {
                if (e.dirty) {
                    int pg = static_cast<int>(&e - &pageTable[0]);
                    writePage(pg, f);
                }
                e.present = false;
                e.frame   = -1;
                e.dirty   = false;
            }
        freeList.push_back(f);
    }

    // load a page from backing store into frame f
    void loadPage(int pg, int f)
    {
        std::ifstream in(storeFile);
        std::string line;

        std::getline(in, line); // skip header
        const std::string tag = std::to_string(pg) + ':';

        while (std::getline(in, line))
            if (line.rfind(tag, 0) == 0) {
                std::istringstream iss(line.substr(tag.size()));
                for (auto& w : frames[f]) iss >> w;
                return;
            }
        std::fill(frames[f].begin(), frames[f].end(), 0); // page not found, proceed to zero fill
    }

    // write the current frame data to backing store
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
            if (line.rfind(tag, 0) == 0) {
                buf << newline.str(); // replace old copy
                replaced = true;
            } else {
                buf << line << '\n';
            }
        }
        if (!replaced) buf << newline.str(); // brand‑new page

        std::ofstream out(storeFile, std::ios::trunc);
        out << buf.str();

        ++nPagesPagedOut;
    }
};

#endif