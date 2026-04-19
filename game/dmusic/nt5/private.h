//      Copyright (c) 1996-1999 Microsoft Corporation
//
//
//
#ifndef _PRIVATE
#define _PRIVATE_

NTSTATUS CreateMiniportDmSynth
(
    OUT PUNKNOWN *  Unknown,
    IN  PUNKNOWN    UnknownOuter OPTIONAL,
    IN  POOL_TYPE   PoolType
);


#include <stdunk.h>

class CDmSynthStream;

class CMiniportDmSynth : public IMiniportSynthesizer, public CUnknown
{
friend class CDmSynthStream;

public:
    // IUnknown
    //
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CMiniportDmSynth);

    ~CMiniportDmSynth();

    // IMiniport
    //
    STDMETHOD(GetDescription)
    (   THIS_
        OUT     PPCFILTER_DESCRIPTOR *  OutFilterDescriptor
    );

    STDMETHOD(DataRangeIntersection)
    (   THIS_
        IN      ULONG           PinId,
        IN      PKSDATARANGE    DataRange,
        IN      PKSDATARANGE    MatchingDataRange,
        IN      ULONG           OutputBufferLength,
        OUT     PVOID           ResultantFormat    OPTIONAL,
        OUT     PULONG          ResultantFormatLength
    );

    // IMiniportDmSynth
    //
    STDMETHOD(Init)
    (   THIS_
        IN      PUNKNOWN        UnknownNotUsed  OPTIONAL,
        IN      PRESOURCELIST   ResourceList,
        IN      PPORTSYNTHESIZER Port,
        OUT     PSERVICEGROUP * ServiceGroup
    );  
      
    STDMETHOD(NewStream)
    (   THIS_
        OUT     PMINIPORTSYNTHESIZERSTREAM *   Stream,
        IN      PUNKNOWN                OuterUnknown    OPTIONAL,
        IN      POOL_TYPE               PoolType,
        IN      ULONG                   Pin,
        IN      BOOLEAN                 Capture,
        IN      PKSDATAFORMAT           DataFormat,
        OUT     PSERVICEGROUP *         ServiceGroup
    );

    STDMETHOD_(void, Service)
    (   void
    );

private:
    PPORTSYNTHESIZER        Port;
    CDmSynthStream *        Stream;    
};


class CDmSynthStream : public IMiniportSynthesizerStream, public CUnknown
{
public:
    // IUnknown
    //
    DECLARE_STD_UNKNOWN();
    DEFINE_STD_CONSTRUCTOR(CDmSynthStream);
    ~CDmSynthStream();
    NTSTATUS Init(CMiniportDmSynth *Miniport);

    // IMxfFilter
    //
    STDMETHOD(ConnectOutput)
    (
        PMXFFILTER ConnectionPoint
    );
    
    STDMETHOD(DisconnectOutput)
    (
        PMXFFILTER ConnectionPoint
    );

    STDMETHOD(PutMessage)
    (   THIS_
        IN  PDMUS_KERNEL_EVENT  Event
    ); 

    // IMiniportSynthStream
    //
    STDMETHOD(SetState)
    (   THIS_
        IN      KSSTATE     State
    );

    // Class
    //
    STDMETHOD(HandlePortParams)
    (   THIS_
        IN      PPCPROPERTY_REQUEST pRequest
    );

public:
    CSynth *                Synth;

private:
    CMiniportDmSynth *      Miniport;
    CSysLink *              Sink;
    SYNTH_PORTPARAMS        PortParams;
};

typedef CDmSynthStream *PDMSYNTHSTREAM;

#endif // _PRIVATE_