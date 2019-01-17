/*
 ==============================================================================
 
 SynthVoice.h
 Created: 19 Mar 2018 1:06:20pm
 Author:  Pablo Cuerno
 
 ==============================================================================
 */

#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include "./SynthSound.h"
#include "./PluginProcessor.h"

class SineVoice : public SynthesiserVoice
{
public:
    
    
    bool canPlaySound (SynthesiserSound* sound) override
    {
        
        return dynamic_cast<SynthSound*>(sound) != nullptr;
        

        

        
        
    }
    //==============================================================================
    
    void startNote (int midiNoteNumber, float velocity, SynthesiserSound* sound, int currentPitchWheelPosition) override
    {
     

        tailInSamples=0;
        tailInSamples2=0;
        
        if(attack==0){ // por seguridad, para evitar ruidos
            attack=1;
        }
        
        attackInSamples = int(attack*sampleRate / 1000.0); // paso de ms a s y de s a samples
        levelAttack = levelmax / attackInSamples; // obtengo el escalón necesario
        attack2InSamples = int(attack2*sampleRate / 1000.0);
        levelAttack2= levelmax2 / attack2InSamples;
        
        decayInSamples = int(decay*sampleRate / 1000.0);
        levelDecay = (levelmax - levelsust) / decayInSamples; // ahora la diferencia de niveles es etnre el punto máximo y el de sustain
        decayInSamplesOrig=decayInSamples;// necesaria para la serie al no ser el decay lineal
        decay2InSamples = int(decay2*sampleRate / 1000.0);
        decayInSamplesOrig2=decay2InSamples;
        levelDecay2 = (levelmax2 - levelsust2) / decay2InSamples;

        if(sine1On||sqre1On)
        {
            frequency = MidiMessage::getMidiNoteInHertz(midiNoteNumber+transpose);
        }
        else{
            frequency=0;
        }
        if(sine2On||sqre2On)
        {
            frequency2 =MidiMessage::getMidiNoteInHertz(midiNoteNumber+transpose2);
        }
        else{
            frequency2=0;
        }
        
        angle = updateAngle (frequency, sampleRate); // para calcular el angulo que tengo que avanzar en cada sample para conseguir el tono
        angle2 = updateAngle(frequency2, sampleRate);
        
        freqSqr1=updateSquare(frequency, sampleRate);// igual para la forma cuadrada
        freqSqr2=updateSquare(frequency2, sampleRate);
        
        adsr = 'a';
        adsr2 ='a';
        
        envAttackInSamples  = int(envAttack*sampleRate / 1000.0);// lo mismo de antes para el envelope del filtro
        envAttackInSamplesOrig = envAttackInSamples;
      
        envDecayInSamples  = int(envDecay*sampleRate / 1000.0);
        envDecayInSamplesOrig = envDecayInSamples;
        
      
        if(envelopeAmount>0){
            limFreq = (20000 - baseFreq)*(envelopeAmount/10.0) + baseFreq;
                   }
        if(envelopeAmount<0){
            limFreq = (baseFreq-10)*(envelopeAmount/10.0) + baseFreq;
            if(limFreq < 10.0){ limFreq = 10.0;}
        }
    
        if((envelopeAmount=!0 )&& (envAttack!=0) && (envDecay!=0)){ // para empezar el envelope, solo si tiene sentido
            adsrFilter='a';
        envAttackIncrement = (limFreq - baseFreq)/envAttackInSamples; // divido ahora la frecuencia del filtro entre los samples para calcular cuanto avanzara el filtro en cada paso
            
        }
        filterFreq=baseFreq;
    }
    
    //==============================================================================
    void stopNote(float velocity, bool allowTailOff) override
    {
        if (adsr == 's') {
            
            adsr = 'r'; // caso simple, acabo de pulsar y si estaba en el sustain paso al release
        }
        else {
            stopflag = 1; // caso en el que la nota ha dejado de ser pulsada en attack o decay
        }
        
        if (adsr2 == 's') {
            adsr2 = 'r';
        }
        else {
            stopflag2 = 1;
        }
        
        
        
       
        
        if(attackInSamples>0) // si se cumple esto es que fue durante el attack donde dejo de ser pulsado, lo acabo y paso al release
        {
            attackInSamples=0;
            adsr='r';
            
            levelsust=level; // importante cambiar esto porque si no pegará un cambio brusco de nivel al empezar release por no estar en el sust que esperaba
        }
        if(attack2InSamples>0)
        {
            attack2InSamples=0;
            adsr2='r';
            levelsust2=level2;
        }
        
       
        tailInSamples= int(tail*sampleRate/1000.0);// los mismos calculos ahora para el release o tail
        tailInSamplesOrig=tailInSamples;
        levelRelease = levelsust / tailInSamples;
        
        tailInSamples2= int(tail2*sampleRate/1000.0);
        tailInSamplesOrig2=tailInSamples2;
        levelRelease2 = levelsust2 / tailInSamples2;
        
        if(envDecayInSamples > 0){ // aqui habria acabado en el decay
            if(envDecayInSamples> tailInSamples){
                envDecayInSamples=tailInSamples;// si tengo mas decay que release lo acorto para que acabe antes, como debería
            }
            
               }
               
        
    }
    //==============================================================================
    // métodos declarados aunque este vacios para no causar problemas con la clase synthVoice
    
    void pitchWheelMoved (int newPitchWheelValue) override
    {
        
    }
    //==============================================================================
    
    void controllerMoved( int controllerNumber, int newControllerValue) override
    {
        
    }
    //==============================================================================
    
    void renderNextBlock ( AudioBuffer<float> &outputBuffer, int startSample, int numSamples) override
    {
        sampleRate = getSampleRate();
        
        delayBufferLength  = (int) 2.0 * sampleRate;
        
         if(envelopeAmount==0){filterFreq=baseFreq;}
        
        //preparo los filtros
        if(FilterLP){
          filter.setCoefficients(IIRCoefficients::makeLowPass (sampleRate, filterFreq, resonance));
        }
        
        if(FilterHP){
            
          filter.setCoefficients(IIRCoefficients::makeHighPass (sampleRate, filterFreq, resonance));
        }
        
        
        int dpr=0;
        int dpw=0;
        float *delayData = delayBuffer;
        delayReadPosition = (int) (delayWritePosition - (delayLength * sampleRate) + delayBufferLength) % delayBufferLength;
        dpr = delayReadPosition;
        dpw = delayWritePosition;
        
        
        for (auto sample = 0; sample < numSamples; ++sample) // lo anterior se hacia una vez por block, ahora esto se hace una vez por sample
        {
            if((envelopeAmount==0 )&& (envAttack==0) && (envDecay==0)){ // por seguridad, una vez más
                filterFreq = baseFreq;           }
            
            if(currentAngle>MathConstants<float>::twoPi) // por intentar aligerar los cálculos, aunque no parece que influya mucho
            {
                currentAngle = currentAngle - MathConstants<float>::twoPi;
            }
            if(currentAngle2>MathConstants<float>::twoPi)
            {
                currentAngle2 = currentAngle2 - MathConstants<float>::twoPi;
            }
            
            currentSample = ((float)std::sin(currentAngle)*level*sine1On*sqrt(2) + (float)square(freqSqr1,&currentSqr)*level*sqre1On)*Osc1On*osc1Vol+ ((float)std::sin(currentAngle2)*level2*sine2On*sqrt(2) +(float)square(freqSqr2,&currentSqr2)*level2*sqre2On)*Osc2On*osc2Vol  ; // fórmula clave de escritura en el sample, antes de ningún efecto
         
            if(FilterOnOff){
               filter.processSamples(&currentSample,1); // proceso el sample con el filtro, ahora queda el sample cortado por el filtro en currentSample
            }
            
            delayData[dpw] = currentSample + (delayData[dpr] * feedback); // copio en la casilla de escritura del buffer de delay el nuevo mas el porcentaje del feedback que debe llevar cuando lo escriba en el de salida 
            
            currentSample = currentSample * dryMix + wetMix* delayData[dpr]; // ahora si, reemplazo el sample a escribir por el mismo mas su delay
            // estudiese los gráficos de la memoria para una compresión mas facil
            
 
            if (++dpr >= delayBufferLength)
                dpr = 0; // si me salgo del buffer empiezo por el principio, nunca tendré delays que no me quepan por propio diseño
            if (++dpw >= delayBufferLength)
                dpw = 0;
          
            
            currentAngle = currentAngle + angle; // avanzo la senuoidales una unidad, en las cuadradas no es necesario debido al funcionamiento de square()
            currentAngle2 = currentAngle2 + angle2 ;
            
            
            
            outputBuffer.addSample(0, startSample, currentSample*mainVolOut ); // aqui escribo en el buffer de salida (realmente lo dejo preparado para cuando escriba todo el block)
            outputBuffer.addSample(1, startSample, currentSample*mainVolOut ); // lo hago en el canal izquierdo y derecho
         
            ++startSample; // clave, siguiendo el modelo de los ejemplos de JUCE realizo la cuenta sobre la misma variable con la que paso la posición
            
           
            
            if (adsr == 'a')
            {
                 attackInSamples = attackInSamples - 1;
                 level = level + levelAttack;
                 if (attackInSamples == 0) // condición de salida
                 {
                     adsr = 'd';
                 }
            
             }
            
             if (adsr == 'd')
             {
                 
                 level = levelsust + pow(decayInSamples/float(decayInSamplesOrig),2)*(levelmax-levelsust); // a diferencia de attack, este no es lineal
                 decayInSamples = decayInSamples - 1;
                 
                 if (decayInSamples <= 0) // condición de salida
                 {
                     
                    adsr = 's';
                    levelsust=level; // para asegurar
                 }
             
             }
             if (adsr == 's')
             {
                 level = levelsust;
                 if (stopflag)
                 { // por si acabó en delay
                     adsr = 'r';
                     stopflag = 0;
                 }
             }
             
             if (adsr == 'r')
             {
                 level =  pow(tailInSamples/float(tailInSamplesOrig),2)*(levelsust); // igual que el delayn no lineal
                 tailInSamples = tailInSamples - 1;
                 
                 if (tailInSamples <= 0|| level<= 0.001) // condiciones de que ha acabado el sonido y por tanto la nota
                 {
                     frequency = 0;
                     currentAngle=0;
                     currentSqr=0;
                     angle=0;
                     level = 0;
                     endflag=1;
                    
                     decay=0;
                     tail=0;
                     levelsust=0;
                     adsr = 'x';
                     
                 }
             }
             // se repite lo mismo para el segundo oscilador, se podría haber ahorrado trabajando con vectores
            if (adsr2 == 'a')
            {
                attack2InSamples = attack2InSamples - 1;
                level2 = level2 + levelAttack2;
                if (attack2InSamples == 0)
                {
                    adsr2 = 'd';
                }
                
            }
            
            if (adsr2 == 'd')
            {
                level2 = levelsust2 + pow(decay2InSamples/float(decayInSamplesOrig2),2)*(levelmax2-levelsust2);
                decay2InSamples = decay2InSamples - 1;
                
                if (decay2InSamples == 0)
                {
                    adsr2 = 's';
                }
                
            }
            if (adsr2 == 's')
            {
                level2 = levelsust2;
                if (stopflag2)
                {
                    adsr2 = 'r';
                    stopflag2 = 0;
                }
            }
            
            if (adsr2 == 'r')
            {
                level2 =  pow(tailInSamples2/float(tailInSamplesOrig2),2)*(levelsust2);
                tailInSamples2 = tailInSamples2 - 1;
                
                if (tailInSamples2 <= 0)
                {
                    frequency2 = 0;
                    currentAngle2=0;
                    currentSqr2=0;
                    angle2=0;
                    level2 = 0;
                    endflag2=1;
                    attack2=0;
                    decay2=0;
                    tail2=0;
                    levelsust2=0;
                    adsr2 = 'x';
                }
            }
            

            
            if((endflag && endflag2)||(endflag && !sine2On)||(endflag2 && !sine1On)){  //cuando acaban los dos, borro la nota
           
                clearCurrentNote();
                
                if(endflag){endflag=0;}
                if(endflag2){endflag2=0;}
                
                
            }
            
            if(level-levelOld > 0.03){
        
            }
            levelOld=level;
        
        // para el envelope del filtro, practicamente igual a lo anterior pero afectando al cut-off, en vez de a level
            
            if(adsrFilter=='a'){
                filterFreq = filterFreq + envAttackIncrement;
                envAttackInSamples = envAttackInSamples-1;
                
                if(envAttackInSamples==0){
                    adsrFilter='d';
                    
                }
            }
            
            if(adsrFilter=='d'){
                filterFreq = baseFreq+ pow(envDecayInSamples/float(envDecayInSamplesOrig),2)*(limFreq-baseFreq);
                envDecayInSamples = envDecayInSamples - 1;
                
                if(envDecayInSamples==0){
                    adsrFilter='x';
                }
            }
        }
        
        delayReadPosition = dpr;
        delayWritePosition = dpw;
        
    }
    //==============================================================================
    
    float updateAngle(float freq, float actualSampleRate){//para calcular el escalon necesario, estudiense los gráficos de la memoria
        
        return (freq * MathConstants<float>::twoPi / actualSampleRate);
        
        
    }
    //==============================================================================
    
    int updateSquare(float freq, float actualSampleRate){// equivalente al anterior para square
        
        if(freq<=0){return 0;}
        
        else{
                return ( int(sampleRate/freq));
        }
    }
    
    //==============================================================================
    
    float square( int samplesSquare, int * auxSqr ){ // equivalente a sin(), pero en este caso se entra con un puntero que apunta a una variable oscilador, para poder llevar las dos cuentas ( uns por osc) a la vez
    
        
        
        if(*auxSqr < samplesSquare){
            
            if(*auxSqr < samplesSquare/2){
               *auxSqr=*auxSqr+1;
                return (1);
            }
            else{
                *auxSqr=*auxSqr+1;
                return(-1);
            }
        
           
        }
        else{
           * auxSqr = 0;
            return(0);
        }
    
        
    }
    //==============================================================================
    
    
    void updateADSR(float attackGui, float decayGui, float sustainGui, float releaseGui, float attackGui2, float decayGui2, float sustainGui2, float releaseGui2, bool sin1on, bool sin2on, bool sqr1On, bool sqr2On, int trans, int trans2,bool osc1On, bool osc2On, bool filterOnOff, float filterFreqValue,float res, bool filterHP, bool filterLP, float envelope,float attackEnvelope, float decayEnvelope, float startFreq, AudioSampleBuffer delayBuff, float drywetDelay, float timeDelay, float feedbackDelay, float mainVol, float oscVol ) { //transferencia de datos de la GUI
        
        
        attack = attackGui;
        decay = decayGui ;
        
        tail = releaseGui;
        attack2 = attackGui2;
        decay2 = decayGui2 ;
        tail2 = releaseGui2;
        sine1On=sin1on;
        sine2On=sin2on;
        sqre1On=sqr1On;
        sqre2On=sqr2On;
        transpose=trans*12;
        transpose2=trans2*12;
        Osc1On=osc1On;
        Osc2On=osc2On;
   
        if(adsr!='r'){
            levelsust = levelmax * (sustainGui/500);}
        if(adsr2!='r'){
            levelsust2 = levelmax * (sustainGui2/500);}
       
        baseFreq =filterFreqValue;
        envelopeAmount=envelope;
        envAttack=attackEnvelope;
        envDecay = decayEnvelope;
        delayLength = (timeDelay/500);
        feedback = feedbackDelay/100;
        dryMix= 1 -drywetDelay;
        wetMix = 1 - dryMix;
        FilterOnOff=filterOnOff;
        FilterHP=filterHP;
        FilterLP=filterLP;
        resonance=res;
        mainVolOut=mainVol;
        osc1Vol= 1.0 - oscVol;
        osc2Vol= oscVol;
     
        
    }
    

    //==============================================================================
    
    bool stopflag = 0; // flag para la salida cuando no se realiza el envelope entero
    bool stopflag2=0;
    
    bool sine1On;// valores de botones
    bool sine2On;
    bool sqre1On ;
    bool sqre2On;
    bool Osc1On;
    bool Osc2On;
    
    float levelmax=0.2; // volumen máximo de salida
    float levelmax2=0.2;
    
    float levelsust ;// de la GUI Sustain
    float levelsust2;
    
    float levelAttack;// escalón del Attack
    float levelAttack2;
    float levelDecay;// escalón del Decay
    float levelDecay2;
    float levelRelease;// escalón del Release
    float levelRelease2;
    
    int bufferSize;// tamaño del buffer, dado por el DAW. Todos los calculos dependen de el
    
    char adsr='x' ;// variable de estados del envelope ADSR
    char adsr2='x';
    
    float tail; // de la GUI Release
    int tailInSamples; // pasado de ms a samples
    int tailInSamplesOrig; // valor original para la función
    
    float tail2;
    int tailInSamples2;
    int tailInSamplesOrig2;
  
    float attack; // de la GUI Attack
    int attackInSamples = 0; // pasado de ms a samples
    
    float attack2;
    int attack2InSamples=0;
    
    
    float decay;// de la GUI Deacy
    int decayInSamples=0;// pasado de ms a samples
    int decayInSamplesOrig=0;// para los calculos
    
    float decay2=0;
    int decay2InSamples=0;
    int decayInSamplesOrig2;

    
    float currentAngle=0;// donde se almacena el ángulo de la onda
    float currentAngle2=0;
    
    float currentSample=0; // el que se va a escribir
    
    
    float frequency; // del tono al que se quiere llegar
    float frequency2;
    
    int transpose=0;// de la GUI transpose
    int transpose2=0;
    
    float detune=0;
    float detune2=0;
    
    float level;// nivel de salida, controlado por el ADSR
    float level2;
    
    float angle=0;// donde se almacena el escalón en grados
    float angle2=0;
   
    float cyclesPerSample;
    float sampleRate; // todos los cálculos dependeran de este, tambien lo fija el DAW
    
    bool endflag=0;// flag para borrar las notas
    bool endflag2=0;
    
    int currentSqr=0;// posición dentro de la onda cuadrada, se pasa como puntero
    int currentSqr2=0;
    float freqSqr1=0;// igual que frequency, pero para el square
    float freqSqr2=0;
    
    float levelOld;// nivel antiguo
    
    int delayBufferLength;// tamaño del buffer de delay
    float delayBuffer[44100*2];// buffer de delay
    int delayWritePosition=0;// indicador dentro del buffer
    int delayReadPosition=0;
    float delayLength; // de la GUI
    float wetMix;// de la GUI dry/wet
    float dryMix;
    float feedback; // hasta 99, de la GUI
    float time;//hasta 500, de la GUI
    
    float filterFreq=1000;// frecuencia real del filtro
    float resonance;// resonancia
    float baseFreq;// frecuencia base del filtro, es la que maraca la GUI
    float envelopeAmount=0 ;// de la GUI, envelope
    float limFreq;// frecuencia máxima a la que se llegara con un envelope en un instante
    float envAttack;// de la GUI
    int envAttackInSamples;// en samples
    int envAttackInSamplesOrig;
    float envAttackIncrement;// escalón
    float envDecay ;// de la GUI
    int envDecayInSamples ;// en samples
    int envDecayInSamplesOrig ;
    bool FilterHP;// botones de la GUI
    bool FilterLP;
    bool FilterOnOff;
    char adsrFilter='x';// contenedor de los estados del filtro
    float mainVolOut;// voumen de salida
    float osc1Vol;// volumen de cada osc
    float osc2Vol;
    IIRFilter filter ;// declaración del objeto filtro
    
private:
    
};



