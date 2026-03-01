#include "ingest.h"
#include <cstring>

template <size_t BackBufferSize, size_t Instruments>
Ingest<BackBufferSize, Instruments>::Ingest(Book<Instruments> &book)
    : seqs(Instruments + 1)
    , book_queue(book.feed)
{
}

template <size_t BackBufferSize, size_t Instruments>
void Ingest<BackBufferSize, Instruments>::feed(PacketIngest const *p)
{
    uint32_t instr = p->instrument_id;

    auto instr_entry = seqs.try_emplace(instr, seqs.size(), back_buffer_);
    if (seqs.size() > Instruments)
    {
        seqs.erase(instr);
        LOG("too many instrs");
        return;
    }
    InstrSeq &seq = instr_entry.first->second;

    bool const ooo_seq = p->seq_no > seq.next_seq && seq.next_seq;
    bool const dup_seq = p->seq_no < seq.next_seq && seq.next_seq;

    if (ooo_seq)
    {
        // seq has been skipped, buffer the packet for later
        size_t const seq_in_buf = p->seq_no & (BackBufferSize - 1UZ);
        // no check, just override and drop old...
        // TODO: recheck this
        std::memcpy(seq.back_buffer_ref + seq_in_buf, p, sizeof(PacketIngest));

        // TODO: send retrasmit for all missing packets
        return;
    }

    if (dup_seq)
    {
        // ignore duplicate packets
        // TODO: replay attack protection or anything here?
        return;
    }

    // Received packet in sequence... push it
    book_queue.store(*p);
    seq.next_seq = p->seq_no + 1;

    // And also any that came ooo ("newer" packets)
    for (size_t const seq_in_buf = seq.next_seq & (BackBufferSize - 1UZ);
         p = seq.back_buffer_ref + seq_in_buf, p->seq_no == seq.next_seq;
         ++seq.next_seq)
    {
        book_queue.store(*p);
        // p.seq_no ^= 1; // invalidate, just to be sure
    }
}

template class Ingest<16, 5>;
