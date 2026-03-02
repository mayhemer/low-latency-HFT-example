#include "ingest.h"
#include <cstring>

template <size_t BackBufferSize, size_t Instruments, typename SPSC>
Ingest<BackBufferSize, Instruments, SPSC>::Ingest(SPSC& spsc)
    : seqs(Instruments + 1), book_queue(spsc)
{
}

template <size_t BackBufferSize, size_t Instruments, typename SPSC>
void Ingest<BackBufferSize, Instruments, SPSC>::feed(PacketIngest const *incoming)
{
    uint32_t instr = incoming->instrument_id;

    auto instr_entry = seqs.try_emplace(instr, seqs.size(), back_buffer_);
    if (seqs.size() > Instruments)
    {
        seqs.erase(instr);
        LOG("too many instrs");
        return;
    }

    InstrSeq &seq = instr_entry.first->second;

    if (ptrdiff_t window = incoming->seq_no - seq.next_seq; window > buffer_size_signed)
    {
        // back buffer just started to overwrite oldest!  shift the expectations.
        // TODO: this is signal for a possible transmitter push-back.
        seq.next_seq += window - BackBufferSize;

        // check if we have packets to consume
        PacketIngest *p;
        for (size_t const seq_in_buf = seq.next_seq & buffer_size_mask;
             p = seq.back_buffer_ref + seq_in_buf, p->seq_no == seq.next_seq;
             ++seq.next_seq)
        {
            if (!book_queue.store(*p))
            {
                break;
            }
        }
    }

    if (incoming->seq_no > seq.next_seq)
    {
        // TODO: send retrasmit for all missing packets
        // seq has been skipped, buffer the packet for later
        size_t const seq_in_buf = incoming->seq_no & buffer_size_mask;
        // no check, just override and drop old...
        std::memcpy(seq.back_buffer_ref + seq_in_buf, incoming, sizeof(PacketIngest));
        return;
    }

    if (incoming->seq_no < seq.next_seq)
    {
        // ignore duplicate packets
        // TODO: replay attack protection or anything here?
        return;
    }

    // Received packet in sequence... try to push it
    if (!book_queue.store(*incoming))
    {
        size_t const seq_in_buf = incoming->seq_no & buffer_size_mask;
        std::memcpy(seq.back_buffer_ref + seq_in_buf, incoming, sizeof(PacketIngest));
        return;
    }

    // incrementing next_seq only when we delivered makes us reuse ooo logic for
    // blocked deliveries
    seq.next_seq = incoming->seq_no + 1;

    // consume all that came ooo ("newer" packets)
    PacketIngest *p;
    for (size_t const seq_in_buf = seq.next_seq & buffer_size_mask;
         p = seq.back_buffer_ref + seq_in_buf, p->seq_no == seq.next_seq;
         ++seq.next_seq)
    {
        if (!book_queue.store(*p))
        {
            break;
        }
    }
}

template class Ingest<16, 5>;
