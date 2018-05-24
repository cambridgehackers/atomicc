
module mkConnectalTop( input  CLK, input  RST_N,
  input  [33 : 0] ReadReq, input  EN_ReadReq, output RDY_ReadReq,
  input  EN_ReadData, output [38 : 0] ReadData, output RDY_ReadData,
  input  [33 : 0] WriteReq, input  EN_WriteReq, output RDY_WriteReq,
  input  [38 : 0] WriteData, input  EN_WriteData, output RDY_WriteData,
  input  EN_WriteDone, output [5 : 0] WriteDone, output RDY_WriteDone,
  output interrupt_0__read);

  reg ctrlPort_0_interruptEnableReg, ctrlPort_1_interruptEnableReg, CMRlastWriteDataSeen;
  reg readFirst, readLast, selectRIndReq, portalRControl, selectWIndReq, portalWControl;
  reg [31 : 0] requestValue, reqInfo;
  reg [9 : 0] readCount;
  reg [4 : 0] readAddr;

  wire indicationNotEmpty, requestNotFull;
  wire RDY_indication, RDY_requestEnq, ReadDataFifo_FULL_N;
  wire reqPortal_EMPTY_N, reqPortal_FULL_N, read_reqFifo_EMPTY_N, read_reqFifo_FULL_N;
  wire reqArs_EMPTY_N, reqArs_FULL_N, reqrs_EMPTY_N, reqrs_FULL_N;
  wire reqwriteDataFifo_EMPTY_N, reqwriteDataFifo_FULL_N, reqws_EMPTY_N;
  wire readFirstNext, readAddr_EN, RULEread;
  wire [38 : 0] reqRead_D_OUT, requestData;
  wire [31 : 0]indIntrChannel, reqIntrChannel, indicationData; 
  wire [21 : 0] reqPortal_D_OUT;
  wire [20 : 0] reqArs_D_OUT, read_reqFifo_D_OUT;
  wire [9 : 0] readburstCount;
  wire [4 : 0] readAddrupdate;
  wire [1 : 0] selectIndication, selectRequest;

  assign interrupt_0__read = (indIntrChannel != 0 && ctrlPort_0_interruptEnableReg) || (reqIntrChannel != 0 && ctrlPort_1_interruptEnableReg );
  assign readFirstNext = readFirst ? read_reqFifo_D_OUT[15:6] == 10'd4  : readLast ;
  assign RULEread = reqPortal_EMPTY_N && ReadDataFifo_FULL_N && (selectRIndReq ?
        (((portalRControl || reqPortal_D_OUT[21:17] != 5'd4) && !reqPortal_D_OUT[0]) || reqrs_EMPTY_N)
       : ((portalRControl || reqPortal_D_OUT[21:17] != 5'd0 || RDY_indication) && reqrs_EMPTY_N));

  always@(reqPortal_D_OUT or indicationData or indicationNotEmpty or reqPortal_D_OUT or requestNotFull)
  begin
    if (selectRIndReq)
      requestValue = { 31'd0, (reqPortal_D_OUT[21:17] == 5'd4 && requestNotFull) };
    else
    case (reqPortal_D_OUT[21:17])
      5'd0: requestValue = indicationData;
      5'd4: requestValue = { 31'd0, indicationNotEmpty };
      default: requestValue = 32'd0;
    endcase
  end
  always@(reqPortal_D_OUT or ctrlPort_0_interruptEnableReg or indIntrChannel or ctrlPort_1_interruptEnableReg or reqIntrChannel)
  begin
    if (selectRIndReq)
    case (reqPortal_D_OUT[21:17])
      5'd0: reqInfo = {31'd0,  reqIntrChannel != 0};
      5'd4: reqInfo = {31'd0, ctrlPort_1_interruptEnableReg};
      5'd8: reqInfo = 32'd1;
      5'h0C: reqInfo = reqIntrChannel;
      5'h10: reqInfo = 32'd6;
      5'h14: reqInfo = 32'd2;
      5'h18: reqInfo = 32'd0;
      5'h1C: reqInfo = 32'd0;
      default: reqInfo = 32'h005A05A0;
    endcase
    else
    case (reqPortal_D_OUT[21:17])
      5'd0: reqInfo = {31'd0,  indIntrChannel != 0};
      5'd4: reqInfo = {31'd0, ctrlPort_0_interruptEnableReg};
      5'd8: reqInfo = 32'd1;
      5'h0C: reqInfo = indIntrChannel;
      5'h10: reqInfo = 32'd5;
      5'h14: reqInfo = 32'd2;
      5'h18: reqInfo = 32'd0;
      5'h1C: reqInfo = 32'd0;
      default: reqInfo = 32'h005A05A0;
    endcase
  end

  assign RDY_ReadReq = !reqArs_EMPTY_N;
  assign readAddr_EN = read_reqFifo_EMPTY_N && reqPortal_FULL_N;

  FIFO1 #(.width(2), .guarded(1)) reqrs(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN(ReadReq[22:21] - 2'd1), .ENQ(EN_ReadReq),
        .D_OUT(selectIndication), .DEQ(RULEread && (!selectRIndReq || reqPortal_D_OUT[0])), .FULL_N(reqrs_FULL_N), .EMPTY_N(reqrs_EMPTY_N));
  FIFO1 #(.width(21), .guarded(1)) reqArs(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN(ReadReq[20:0]), .ENQ(EN_ReadReq),
        .D_OUT(reqArs_D_OUT), .DEQ(reqArs_EMPTY_N), .FULL_N(reqArs_FULL_N), .EMPTY_N(reqArs_EMPTY_N));
  always@(posedge CLK) begin
        if (EN_ReadReq) begin
            portalRControl <= ReadReq[27:21] == 7'd0;
            selectRIndReq <= ReadReq[28];
        end
  end
  FIFO1 #(.width(21), .guarded(1)) read_reqFifo(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN(reqArs_D_OUT), .ENQ(reqArs_EMPTY_N && read_reqFifo_FULL_N),
        .D_OUT(read_reqFifo_D_OUT), .DEQ(readAddr_EN && readFirstNext), .FULL_N(read_reqFifo_FULL_N), .EMPTY_N(read_reqFifo_EMPTY_N));
  assign readAddrupdate = readFirst ?  read_reqFifo_D_OUT[20:16] : readAddr ;
  assign readburstCount = readFirst ?  { 2'd0, read_reqFifo_D_OUT[15:8] } : readCount ;
  FIFO2 #(.width(22), .guarded(1)) reqPortal(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN({readAddrupdate, readburstCount, read_reqFifo_D_OUT[5:0], readFirstNext}), .ENQ(readAddr_EN),
        .D_OUT(reqPortal_D_OUT), .DEQ(RULEread), .FULL_N(reqPortal_FULL_N), .EMPTY_N(reqPortal_EMPTY_N));
  FIFO2 #(.width(39), .guarded(1)) ReadDataFifo(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN({portalRControl ? reqInfo : requestValue, reqPortal_D_OUT[6:0]}), .ENQ(RULEread),
        .D_OUT(ReadData), .DEQ(EN_ReadData), .FULL_N(ReadDataFifo_FULL_N), .EMPTY_N(RDY_ReadData));

//write
  reg writeFirst, writeLast;
  reg [9 : 0] writeCount;
  reg [4 : 0] writeAddr;

  wire writeAddr_EN, reqws_FULL_N, writeFirstNext, WriteDone_FULL_N, RULEwrite;
  wire writeFifo_EMPTY_N, writeFifo_FULL_N, write_reqFifo_EMPTY_N, write_reqFifo_FULL_N, reqdoneFifo_ENQ;
  wire [27 : 0] ctrlAws_D_OUT;
  wire [21 : 0] writeFifo_D_OUT;
  wire [20 : 0] write_reqFifo_D_OUT;
  wire [9 : 0] writeburstCount;
  wire [4 : 0] writeAddrupdate;

  assign RULEwrite = reqwriteDataFifo_EMPTY_N && writeFifo_EMPTY_N && (!writeFifo_D_OUT[0] || WriteDone_FULL_N)
            && (!selectWIndReq || portalWControl || (reqws_EMPTY_N && RDY_requestEnq));
  assign writeAddr_EN = write_reqFifo_EMPTY_N && writeFifo_FULL_N ;

  assign RDY_WriteReq = reqws_FULL_N && write_reqFifo_FULL_N;
  assign RDY_WriteData = !CMRlastWriteDataSeen && reqwriteDataFifo_FULL_N ;
  assign reqdoneFifo_ENQ = RULEwrite && writeFifo_D_OUT[0];
  assign writeFirstNext = writeFirst ?  write_reqFifo_D_OUT[15:6] == 10'd4 : writeLast ;
  FIFO1 #(.width(21), .guarded(1)) write_reqFifo(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN(WriteReq[20:0]), .ENQ(EN_WriteReq),
        .D_OUT(write_reqFifo_D_OUT), .DEQ(writeAddr_EN && writeFirstNext), .FULL_N(write_reqFifo_FULL_N), .EMPTY_N(write_reqFifo_EMPTY_N));
  FIFO1 #(.width(2), .guarded(1)) reqws(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN(WriteReq[22:21] - 2'd1), .ENQ(EN_WriteReq),
        .D_OUT(selectRequest), .DEQ(reqdoneFifo_ENQ), .FULL_N(reqws_FULL_N), .EMPTY_N(reqws_EMPTY_N));
  always@(posedge CLK) begin
        if (EN_WriteReq) begin
            portalWControl <= WriteReq[27:21] == 7'd0;
            selectWIndReq <= WriteReq[28];
        end
  end
  FIFO2 #(.width(39), .guarded(1)) reqwriteDataFifo(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN(WriteData), .ENQ(EN_WriteData),
        .D_OUT(requestData), .DEQ(RULEwrite), .FULL_N(reqwriteDataFifo_FULL_N), .EMPTY_N(reqwriteDataFifo_EMPTY_N));
  assign writeAddrupdate = writeFirst ?  write_reqFifo_D_OUT[20:16] : writeAddr ;
  assign writeburstCount = writeFirst ?  { 2'd0, write_reqFifo_D_OUT[15:8] } : writeCount ;
  FIFO2 #(.width(22), .guarded(1)) writeFifo(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN({ writeAddrupdate, writeburstCount, write_reqFifo_D_OUT[5:0], writeFirstNext }), .ENQ(writeAddr_EN),
        .D_OUT(writeFifo_D_OUT), .DEQ(RULEwrite), .FULL_N(writeFifo_FULL_N), .EMPTY_N(writeFifo_EMPTY_N));
  FIFO1 #(.width(6), .guarded(1)) CMRdoneFifo(.RST(RST_N), .CLK(CLK), .CLR(0),
        .D_IN(writeFifo_D_OUT[6:1]), .ENQ(reqdoneFifo_ENQ),
        .D_OUT(WriteDone), .DEQ(EN_WriteDone), .FULL_N(WriteDone_FULL_N), .EMPTY_N(RDY_WriteDone));

  always@(posedge CLK)
  begin
    if (RST_N == 0)
      begin
        ctrlPort_0_interruptEnableReg <=  1'd0;
        ctrlPort_1_interruptEnableReg <=  1'd0;
        CMRlastWriteDataSeen <=  1'd0;
        readAddr <= 5'd0;
        readCount <= 10'd0;
        readFirst <= 1'd1;
        readLast <= 1'd0;
        writeAddr <= 5'd0;
        writeCount <= 10'd0;
        writeFirst <= 1'd1;
        writeLast <= 1'd0;
      end
    else
      begin
        if (RULEwrite && portalWControl && writeFifo_D_OUT[21:17] == 5'd4) begin
          if (selectWIndReq)
            ctrlPort_1_interruptEnableReg <= requestData[7];
          else
            ctrlPort_0_interruptEnableReg <= requestData[7];
        end
        if (EN_WriteData && WriteData[0] || EN_WriteReq)
          CMRlastWriteDataSeen <= !EN_WriteReq;
        if (readAddr_EN) begin
          readAddr <= readAddrupdate + 5'd4 ;
          readCount <= readburstCount - 10'd1 ;
          readFirst <= readFirstNext;
          readLast <= readburstCount == 10'd2 ;
        end

        if (writeAddr_EN) begin
          writeAddr <= writeAddrupdate + 5'd4 ;
          writeCount <= writeburstCount - 10'd1 ;
          writeFirst <= writeFirstNext ;
          writeLast <= writeburstCount == 10'd2 ;
        end
      end
  end
mkHACK havvvv(.CLK(CLK), .RST_N(RST_N),
    .selectIndication(selectIndication), .selectRequest(selectRequest),
    .requestEnqV(requestData[38:7]),
    .EN_indication(RULEread && !portalRControl && reqPortal_D_OUT[21:17] == 5'd0),
    .EN_request(RULEwrite && !portalWControl && selectWIndReq),
    .reqIntrChannel(reqIntrChannel), .indIntrChannel(indIntrChannel),
    .requestNotFull(requestNotFull),
    .indicationNotEmpty(indicationNotEmpty), .indicationData(indicationData),
    .RDY_indication(RDY_indication),
    .RDY_requestEnq(RDY_requestEnq));
endmodule  // mkConnectalTop

module mkHACK(input CLK, input  RST_N,
    input [1:0]selectIndication, input [1:0]selectRequest,
    input [31 : 0] requestEnqV,
    input EN_indication, input EN_request,
    output [31 : 0]reqIntrChannel, output [31 : 0]indIntrChannel,
    output requestNotFull,
    output indicationNotEmpty,
    output [31 : 0] indicationData,
    output RDY_indication,
    output RDY_requestEnq);

  reg RDY_requestEnq, requestNotFull;
  wire RDY_indicationDeq, RDY_indicationData, indicationNotEmpty;
  wire [31 : 0] indicationData;

  wire RDY_req0_enq, RDY_req1_enq, RDY_req2_enq, ind0_notEmpty, ind1_notEmpty;
  wire reqIntrStatus, indIntrStatus;
  wire [31 : 0]ind0_first, ind1_first;
  wire [31 : 0]dutreqIntrChannel, dutindIntrChannel;

  always@(selectRequest or RDY_req0_enq or RDY_req1_enq or RDY_req2_enq)
  begin
    case (selectRequest)
      2'd0: RDY_requestEnq = RDY_req0_enq;
      2'd1: RDY_requestEnq = RDY_req1_enq;
      2'd2: RDY_requestEnq = RDY_req2_enq;
      2'd3: RDY_requestEnq = 1'd1;
    endcase
  end
  always@(selectIndication or req0_notFull or req1_notFull or req2_notFull or
      ind0_first or ind1_first or RDY_ind0_first or RDY_ind1_first or
      ind0_notEmpty or ind1_notEmpty or RDY_ind0_deq or RDY_ind1_deq)
  begin
    case (selectIndication)
      2'd0: requestNotFull = req0_notFull;
      2'd1: requestNotFull = req1_notFull;
      2'd2: requestNotFull = req2_notFull;
      2'd3: requestNotFull = 1'b0 /* unspecified value */ ;
    endcase
  end
  assign indicationNotEmpty = selectIndication[0] ? ind1_notEmpty : ind0_notEmpty;
  assign indicationData = selectIndication[0] ? ind1_first : ind0_first;
  assign RDY_indicationData = selectIndication[0] ? RDY_ind1_first : RDY_ind0_first;
  assign RDY_indicationDeq = selectIndication[0] ? RDY_ind1_deq : RDY_ind0_deq;
  assign RDY_indication = RDY_indicationDeq && RDY_indicationData;
  assign reqIntrChannel = reqIntrStatus ? (dutreqIntrChannel + 32'd1) : 32'd0;
  assign indIntrChannel = indIntrStatus ? (dutindIntrChannel + 32'd1) : 32'd0;
////////////////////////////////////////////////////////////////////////////
  wire [31 : 0] lEchoIndicationOutput_ifc_heard_v;
  wire [15 : 0] lEchoIndicationOutput_ifc_heard2_a,
                lEchoIndicationOutput_ifc_heard2_b,
                lEchoIndicationOutput_portalIfc_messageSize_size_methodNumber;
  wire EN_lEchoIndicationOutput_ifc_heard,
       EN_lEchoIndicationOutput_ifc_heard2,
       EN_lEchoIndicationOutput_portalIfc_indications_0_deq,
       EN_lEchoIndicationOutput_portalIfc_indications_1_deq,
       RDY_lEchoIndicationOutput_ifc_heard,
       RDY_lEchoIndicationOutput_ifc_heard2,
       RDY_ind0_deq, RDY_ind1_deq,
       RDY_ind0_first, RDY_ind1_first;
  wire [31 : 0] lEchoRequestInput_pipes_say2_PipeOut_first,
                lEchoRequestInput_pipes_say_PipeOut_first;
  wire [15 : 0] lEchoRequestInput_portalIfc_messageSize_size_methodNumber;
  wire EN_lEchoRequestInput_pipes_say2_PipeOut_deq,
       EN_lEchoRequestInput_pipes_say_PipeOut_deq,
       EN_lEchoRequestInput_pipes_setLeds_PipeOut_deq,
       EN_lEchoRequestInput_portalIfc_requests_0_enq,
       EN_lEchoRequestInput_portalIfc_requests_1_enq,
       EN_lEchoRequestInput_portalIfc_requests_2_enq,
       RDY_lEchoRequestInput_pipes_say2_PipeOut_deq,
       RDY_lEchoRequestInput_pipes_say2_PipeOut_first,
       RDY_lEchoRequestInput_pipes_say_PipeOut_deq,
       RDY_lEchoRequestInput_pipes_say_PipeOut_first,
       RDY_lEchoRequestInput_pipes_setLeds_PipeOut_deq,
       req0_notFull, req1_notFull, req2_notFull;
  wire [31 : 0] lEcho_delay_D_IN, lEcho_delay_D_OUT;
  wire lEcho_delay_DEQ, lEcho_delay_EMPTY_N, lEcho_delay_ENQ, lEcho_delay_FULL_N;
  wire [31 : 0] lEcho_delay2_D_IN, lEcho_delay2_D_OUT;
  wire lEcho_delay2_DEQ, lEcho_delay2_EMPTY_N, lEcho_delay2_ENQ, lEcho_delay2_FULL_N;
  assign lEchoIndicationOutput_ifc_heard2_a = lEcho_delay2_D_OUT[15:0] ;
  assign lEchoIndicationOutput_ifc_heard2_b = lEcho_delay2_D_OUT[31:16] ;
  assign lEchoIndicationOutput_ifc_heard_v = lEcho_delay_D_OUT ;
  assign lEchoIndicationOutput_portalIfc_messageSize_size_methodNumber =
             16'h0 ;
  assign EN_lEchoIndicationOutput_portalIfc_indications_0_deq = EN_indication && selectIndication[0] == 1'd0;
  assign EN_lEchoIndicationOutput_portalIfc_indications_1_deq = EN_indication && selectIndication[0] == 1'd1;
  assign EN_lEchoIndicationOutput_ifc_heard =
             RDY_lEchoIndicationOutput_ifc_heard && lEcho_delay_EMPTY_N ;
  assign EN_lEchoIndicationOutput_ifc_heard2 =
             RDY_lEchoIndicationOutput_ifc_heard2 && lEcho_delay2_EMPTY_N ;
  assign lEchoRequestInput_portalIfc_messageSize_size_methodNumber = 16'h0 ;
  assign EN_lEchoRequestInput_portalIfc_requests_0_enq = EN_request && selectRequest == 2'd0;
  assign EN_lEchoRequestInput_portalIfc_requests_1_enq = EN_request && selectRequest == 2'd1;
  assign EN_lEchoRequestInput_portalIfc_requests_2_enq = EN_request && selectRequest == 2'd2;
  assign EN_lEchoRequestInput_pipes_say_PipeOut_deq = RDY_lEchoRequestInput_pipes_say_PipeOut_first && RDY_lEchoRequestInput_pipes_say_PipeOut_deq && lEcho_delay_FULL_N ;
  assign EN_lEchoRequestInput_pipes_say2_PipeOut_deq = RDY_lEchoRequestInput_pipes_say2_PipeOut_first && RDY_lEchoRequestInput_pipes_say2_PipeOut_deq && lEcho_delay2_FULL_N ;
  assign EN_lEchoRequestInput_pipes_setLeds_PipeOut_deq = RDY_lEchoRequestInput_pipes_setLeds_PipeOut_deq ;
  assign lEcho_delay_D_IN = lEchoRequestInput_pipes_say_PipeOut_first ;
  assign lEcho_delay_ENQ = RDY_lEchoRequestInput_pipes_say_PipeOut_first && RDY_lEchoRequestInput_pipes_say_PipeOut_deq && lEcho_delay_FULL_N ;
  assign lEcho_delay_DEQ = RDY_lEchoIndicationOutput_ifc_heard && lEcho_delay_EMPTY_N ;
  assign lEcho_delay2_D_IN = lEchoRequestInput_pipes_say2_PipeOut_first ;
  assign lEcho_delay2_ENQ = RDY_lEchoRequestInput_pipes_say2_PipeOut_first && RDY_lEchoRequestInput_pipes_say2_PipeOut_deq && lEcho_delay2_FULL_N ;
  assign lEcho_delay2_DEQ = RDY_lEchoIndicationOutput_ifc_heard2 && lEcho_delay2_EMPTY_N ;

  mkEchoIndicationOutput lEchoIndicationOutput(.CLK(CLK),
        .RST_N(RST_N),
        .ifc_heard2_a(lEchoIndicationOutput_ifc_heard2_a),
        .ifc_heard2_b(lEchoIndicationOutput_ifc_heard2_b),
        .ifc_heard_v(lEchoIndicationOutput_ifc_heard_v),
        .portalIfc_messageSize_size_methodNumber(lEchoIndicationOutput_portalIfc_messageSize_size_methodNumber),
        .EN_portalIfc_indications_0_deq(EN_lEchoIndicationOutput_portalIfc_indications_0_deq),
        .EN_portalIfc_indications_1_deq(EN_lEchoIndicationOutput_portalIfc_indications_1_deq),
        .EN_ifc_heard(EN_lEchoIndicationOutput_ifc_heard),
        .EN_ifc_heard2(EN_lEchoIndicationOutput_ifc_heard2),
        .portalIfc_messageSize_size(),
        .RDY_portalIfc_messageSize_size(),
        .portalIfc_indications_0_first(ind0_first),
        .RDY_portalIfc_indications_0_first(RDY_ind0_first),
        .RDY_portalIfc_indications_0_deq(RDY_ind0_deq),
        .portalIfc_indications_0_notEmpty(ind0_notEmpty),
        .RDY_portalIfc_indications_0_notEmpty(),
        .portalIfc_indications_1_first(ind1_first),
        .RDY_portalIfc_indications_1_first(RDY_ind1_first),
        .RDY_portalIfc_indications_1_deq(RDY_ind1_deq),
        .portalIfc_indications_1_notEmpty(ind1_notEmpty),
        .RDY_portalIfc_indications_1_notEmpty(),
        .portalIfc_intr_status(indIntrStatus),
        .RDY_portalIfc_intr_status(),
        .portalIfc_intr_channel(dutindIntrChannel),
        .RDY_portalIfc_intr_channel(),
        .RDY_ifc_heard(RDY_lEchoIndicationOutput_ifc_heard),
        .RDY_ifc_heard2(RDY_lEchoIndicationOutput_ifc_heard2));
  mkEchoRequestInput lEchoRequestInput(.CLK(CLK),
        .RST_N(RST_N),
        .portalIfc_messageSize_size_methodNumber(lEchoRequestInput_portalIfc_messageSize_size_methodNumber),
        .portalIfc_requests_0_enq_v(requestEnqV),
        .portalIfc_requests_1_enq_v(requestEnqV),
        .portalIfc_requests_2_enq_v(requestEnqV),
        .EN_portalIfc_requests_0_enq(EN_lEchoRequestInput_portalIfc_requests_0_enq),
        .EN_portalIfc_requests_1_enq(EN_lEchoRequestInput_portalIfc_requests_1_enq),
        .EN_portalIfc_requests_2_enq(EN_lEchoRequestInput_portalIfc_requests_2_enq),
        .EN_pipes_say_PipeOut_deq(EN_lEchoRequestInput_pipes_say_PipeOut_deq),
        .EN_pipes_say2_PipeOut_deq(EN_lEchoRequestInput_pipes_say2_PipeOut_deq),
        .EN_pipes_setLeds_PipeOut_deq(EN_lEchoRequestInput_pipes_setLeds_PipeOut_deq),
        .portalIfc_messageSize_size(),
        .RDY_portalIfc_messageSize_size(),
        .RDY_portalIfc_requests_0_enq(RDY_req0_enq),
        .portalIfc_requests_0_notFull(req0_notFull),
        .RDY_portalIfc_requests_0_notFull(),
        .RDY_portalIfc_requests_1_enq(RDY_req1_enq),
        .portalIfc_requests_1_notFull(req1_notFull),
        .RDY_portalIfc_requests_1_notFull(),
        .RDY_portalIfc_requests_2_enq(RDY_req2_enq),
        .portalIfc_requests_2_notFull(req2_notFull),
        .RDY_portalIfc_requests_2_notFull(),
        .portalIfc_intr_status(reqIntrStatus),
        .RDY_portalIfc_intr_status(),
        .portalIfc_intr_channel(dutreqIntrChannel),
        .RDY_portalIfc_intr_channel(),
        .pipes_say_PipeOut_first(lEchoRequestInput_pipes_say_PipeOut_first),
        .RDY_pipes_say_PipeOut_first(RDY_lEchoRequestInput_pipes_say_PipeOut_first),
        .RDY_pipes_say_PipeOut_deq(RDY_lEchoRequestInput_pipes_say_PipeOut_deq),
        .pipes_say_PipeOut_notEmpty(),
        .RDY_pipes_say_PipeOut_notEmpty(),
        .pipes_say2_PipeOut_first(lEchoRequestInput_pipes_say2_PipeOut_first),
        .RDY_pipes_say2_PipeOut_first(RDY_lEchoRequestInput_pipes_say2_PipeOut_first),
        .RDY_pipes_say2_PipeOut_deq(RDY_lEchoRequestInput_pipes_say2_PipeOut_deq),
        .pipes_say2_PipeOut_notEmpty(),
        .RDY_pipes_say2_PipeOut_notEmpty(),
        .pipes_setLeds_PipeOut_first(),
        .RDY_pipes_setLeds_PipeOut_first(),
        .RDY_pipes_setLeds_PipeOut_deq(RDY_lEchoRequestInput_pipes_setLeds_PipeOut_deq),
        .pipes_setLeds_PipeOut_notEmpty(),
        .RDY_pipes_setLeds_PipeOut_notEmpty());
  SizedFIFO #(.p1width(32), .p2depth(8), .p3cntr_width(3), .guarded(1)) lEcho_delay(.RST(RST_N), .CLK(CLK),
        .D_IN(lEcho_delay_D_IN),
        .ENQ(lEcho_delay_ENQ),
        .DEQ(lEcho_delay_DEQ),
        .CLR(0),
        .D_OUT(lEcho_delay_D_OUT),
        .FULL_N(lEcho_delay_FULL_N),
        .EMPTY_N(lEcho_delay_EMPTY_N));
  SizedFIFO #(.p1width(32), .p2depth(8), .p3cntr_width(3), .guarded(1)) lEcho_delay2(.RST(RST_N), .CLK(CLK),
        .D_IN(lEcho_delay2_D_IN),
        .ENQ(lEcho_delay2_ENQ),
        .DEQ(lEcho_delay2_DEQ),
        .CLR(0),
        .D_OUT(lEcho_delay2_D_OUT),
        .FULL_N(lEcho_delay2_FULL_N),
        .EMPTY_N(lEcho_delay2_EMPTY_N));
endmodule
