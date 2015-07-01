/**
 *  @file   LArContent/src/LArMonitoring/ParticleMonitoringAlgorithm.cc
 *
 *  @brief  Implementation of the particle monitoring algorithm.
 *
 *  $Log: $
 */

#include "Pandora/AlgorithmHeaders.h"

#include "LArHelpers/LArClusterHelper.h"
#include "LArHelpers/LArGeometryHelper.h"
#include "LArHelpers/LArMCParticleHelper.h"
#include "LArHelpers/LArPfoHelper.h"

#include "LArMonitoring/ParticleMonitoringAlgorithm.h"

#include "LArObjects/LArThreeDSlidingFitResult.h"

using namespace pandora;

namespace lar_content
{

ParticleMonitoringAlgorithm::ParticleMonitoringAlgorithm() :
    m_useDaughterPfos(false),
    m_extractNeutrinoDaughters(true)
{
}

//------------------------------------------------------------------------------------------------------------------------------------------

ParticleMonitoringAlgorithm::~ParticleMonitoringAlgorithm()
{
    PANDORA_MONITORING_API(SaveTree(this->GetPandora(), m_treeName.c_str(), m_fileName.c_str(), "UPDATE"));
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode ParticleMonitoringAlgorithm::Run()
{
#ifdef MONITORING
    // Tree elements
    int nMCParticlesTotal(0), nPfosTotal(0);
    IntVector mcPdgVector, mcNeutrinoVector, nMCHitsVector, pfoPdgVector, pfoNeutrinoVector, nPfoHitsVector, nMatchedHitsVector, 
        nMCHitsUVector, nPfoHitsUVector, nMatchedHitsUVector, nMCHitsVVector, nPfoHitsVVector, nMatchedHitsVVector, nMCHitsWVector, 
        nPfoHitsWVector, nMatchedHitsWVector, pfoVertexVector, pfoVertexDirVector;
    FloatVector completenessVector, purityVector, mcPxVector, mcPyVector, mcPzVector, mcThetaVector, mcEnergyVector, mcPTotVector,
        mcVtxXPosVector, mcVtxYPosVector, mcVtxZPosVector, mcEndXPosVector, mcEndYPosVector, mcEndZPosVector,
        pfoPxVector, pfoPyVector, pfoPzVector, pfoPTotVector, pfoVtxXVector, pfoVtxYVector, pfoVtxZVector,
        // QUICK AND NASTY HACK!!! (TODO: FIX THE QUICK AND NASTY HACK)
        pfoVtxXPosVector, pfoVtxYPosVector, pfoVtxZPosVector, pfoVtxXDirVector, pfoVtxYDirVector, pfoVtxZDirVector;
        // QUICK AND NASTY HACK!!!

    try
    {
        // Load List of MC particles
        const MCParticleList *pMCParticleList = NULL;
        (void) PandoraContentApi::GetList(*this, m_mcParticleListName, pMCParticleList);

        if (NULL == pMCParticleList)
        {
            std::cout << "ParticleMonitoringAlgorithm: cannot find mc particle list " << m_mcParticleListName << std::endl;
            throw StatusCodeException(STATUS_CODE_NOT_FOUND);
        }

        // Load List of Calo Hits
        const CaloHitList *pCaloHitList = NULL;
        (void) PandoraContentApi::GetList(*this, m_caloHitListName, pCaloHitList);

        if (NULL == pCaloHitList)
        {
            std::cout << "ParticleMonitoringAlgorithm: cannot find calo hit list " << m_caloHitListName << std::endl;
            throw StatusCodeException(STATUS_CODE_NOT_FOUND);
        }

        // Load List of Pfos
        const PfoList *pPfoList = NULL;
        PfoList pfoList((STATUS_CODE_SUCCESS == PandoraContentApi::GetList(*this, m_pfoListName, pPfoList)) ? PfoList(*pPfoList) : PfoList());

        if (m_extractNeutrinoDaughters)
            this->ExtractNeutrinoDaughters(pfoList);

        nPfosTotal = pfoList.size();

        // Match Pfos and MC Particles
        MCRelationMap mcPrimaryMap;      // [particles -> primary particle]
        this->GetMCParticleMaps(pMCParticleList, mcPrimaryMap);

        MCContributionMap trueHitMap;    // [primary particle -> true hit list]
        CaloHitToMCMap mcHitMap;         // [hit -> parent primary]
        this->GetMCParticleToCaloHitMatches(pCaloHitList, mcPrimaryMap, mcHitMap, trueHitMap);

        PfoContributionMap recoHitMap;   // [pfo -> reco hit list]
        CaloHitToPfoMap pfoHitMap;       // [hit -> parent pfo]
        this->GetPfoToCaloHitMatches(pCaloHitList, pfoList, pfoHitMap, recoHitMap);

        MCContributionMap matchedHitMap; // [particle -> list of matched hits with best pfo]
        MCToPfoMap matchedPfoMap;        // [particle -> best pfo]
        this->GetMCParticleToPfoMatches(pCaloHitList, pfoList, mcHitMap, matchedPfoMap, matchedHitMap);

        nMCParticlesTotal = trueHitMap.size();

        // Write out reco/true information
        for (MCContributionMap::const_iterator iter = trueHitMap.begin(), iterEnd = trueHitMap.end(); iter != iterEnd; ++iter)
        {
            const MCParticle *const pMCParticle3D(iter->first);
            const CaloHitList &mcHitList(iter->second);

            mcNeutrinoVector.push_back(LArMCParticleHelper::GetParentNeutrinoId(pMCParticle3D));
            mcPdgVector.push_back(pMCParticle3D->GetParticleId());
            mcPxVector.push_back(pMCParticle3D->GetMomentum().GetX());
            mcPyVector.push_back(pMCParticle3D->GetMomentum().GetY());
            mcPzVector.push_back(pMCParticle3D->GetMomentum().GetZ());
            mcPTotVector.push_back(pMCParticle3D->GetMomentum().GetMagnitude());
            mcEnergyVector.push_back(pMCParticle3D->GetEnergy());
            mcThetaVector.push_back(pMCParticle3D->GetMomentum().GetOpeningAngle(CartesianVector(0.f, 0.f, 1.f)));

            mcVtxXPosVector.push_back(pMCParticle3D->GetVertex().GetX());
            mcVtxYPosVector.push_back(pMCParticle3D->GetVertex().GetY());
            mcVtxZPosVector.push_back(pMCParticle3D->GetVertex().GetZ());
            mcEndXPosVector.push_back(pMCParticle3D->GetEndpoint().GetX());
            mcEndYPosVector.push_back(pMCParticle3D->GetEndpoint().GetY());
            mcEndZPosVector.push_back(pMCParticle3D->GetEndpoint().GetZ());

            const int nMCHits(mcHitList.size());
            const int nMCHitsU(this->CountHitsByType(TPC_VIEW_U, mcHitList));
            const int nMCHitsV(this->CountHitsByType(TPC_VIEW_V, mcHitList));
            const int nMCHitsW(this->CountHitsByType(TPC_VIEW_W, mcHitList));

            int pfoVertex(0), pfoPdg(0), pfoNeutrinoPdg(0), nPfoHits(0), nPfoHitsU(0), nPfoHitsV(0), nPfoHitsW(0);
            float pfoPTot(0.f), pfoPx(0.f), pfoPy(0.f), pfoPz(0.f), pfoVtxX(0.f), pfoVtxY(0.f), pfoVtxZ(0.f),
            // QUICK AND NASTY HACK!!!
                pfoVtxXPos(0.f), pfoVtxYPos(0.f), pfoVtxZPos(0.f), pfoVtxXDir(0.f), pfoVtxYDir(0.f), pfoVtxZDir(0.f);
            int pfoVertexDir(0);
            // QUICK AND NASTY HACK!!!
            MCToPfoMap::const_iterator pIter1 = matchedPfoMap.find(pMCParticle3D);

            if (matchedPfoMap.end() != pIter1)
            {
                const ParticleFlowObject *const pPfo(pIter1->second);
                PfoContributionMap::const_iterator pIter2 = recoHitMap.find(pPfo);

                if (recoHitMap.end() == pIter2)
                    throw StatusCodeException(STATUS_CODE_FAILURE);

                pfoPdg = pPfo->GetParticleId();
                pfoNeutrinoPdg = LArPfoHelper::GetPrimaryNeutrino(pPfo);

                pfoPTot = pPfo->GetMomentum().GetMagnitude();
                pfoPx = pPfo->GetMomentum().GetX(); 
                pfoPy = pPfo->GetMomentum().GetY(); 
                pfoPz = pPfo->GetMomentum().GetZ(); 

                if (pPfo->GetVertexList().size() > 0)
                {
                    if (pPfo->GetVertexList().size() != 1)
                        throw StatusCodeException(STATUS_CODE_FAILURE);

                    const Vertex *const pVertex = *(pPfo->GetVertexList().begin());

                    pfoVertex = 1;
                    pfoVtxX = pVertex->GetPosition().GetX();
                    pfoVtxY = pVertex->GetPosition().GetY(); 
                    pfoVtxZ = pVertex->GetPosition().GetZ();

                    // QUICK AND NASTY HACK!!!
                    try
                    {
                        ClusterList clusterList;
                        LArPfoHelper::GetClusters(pPfo, TPC_3D, clusterList);

                        if (clusterList.size() != 1)
                            throw StatusCodeException(STATUS_CODE_FAILURE);

                        const Cluster *const pCluster = *(clusterList.begin());
                        const ThreeDSlidingFitResult slidingFit(pCluster, 20, LArGeometryHelper::GetWireZPitch(this->GetPandora()));

                        const CartesianVector vertexPos(pfoVtxX, pfoVtxY, pfoVtxZ);
                        const CartesianVector minLayerPos(slidingFit.GetGlobalMinLayerPosition());
                        const CartesianVector maxLayerPos(slidingFit.GetGlobalMaxLayerPosition());
                        const CartesianVector minLayerDir(slidingFit.GetGlobalMinLayerDirection());
                        const CartesianVector maxLayerDir(slidingFit.GetGlobalMaxLayerDirection());

                        const CartesianVector &chosenPos(((vertexPos - minLayerPos).GetMagnitudeSquared() < (vertexPos - maxLayerPos).GetMagnitudeSquared()) ?
                            minLayerPos : maxLayerPos);
                        const CartesianVector &chosenDir(((vertexPos - minLayerPos).GetMagnitudeSquared() < (vertexPos - maxLayerPos).GetMagnitudeSquared()) ?
                            minLayerDir : maxLayerDir);

                        pfoVertexDir = 1;
                        pfoVtxXDir = chosenDir.GetX();
                        pfoVtxYDir = chosenDir.GetY(); 
                        pfoVtxZDir = chosenDir.GetZ();
                        pfoVtxXPos = chosenPos.GetX();
                        pfoVtxYPos = chosenPos.GetY(); 
                        pfoVtxZPos = chosenPos.GetZ();
                    }
                    catch (StatusCodeException &)
                    {
                    }
                    // QUICK AND NASTY HACK!!!
                }

                const CaloHitList &pfoHitList(pIter2->second);
                nPfoHits = pfoHitList.size();
                nPfoHitsU = this->CountHitsByType(TPC_VIEW_U, pfoHitList);
                nPfoHitsV = this->CountHitsByType(TPC_VIEW_V, pfoHitList);
                nPfoHitsW = this->CountHitsByType(TPC_VIEW_W, pfoHitList);
            }

            int nMatchedHits(0), nMatchedHitsU(0), nMatchedHitsV(0), nMatchedHitsW(0);
            MCContributionMap::const_iterator mIter = matchedHitMap.find(pMCParticle3D);

            if (matchedHitMap.end() != mIter)
            {
                const CaloHitList &matchedHitList(mIter->second);
                nMatchedHits = matchedHitList.size();
                nMatchedHitsU = this->CountHitsByType(TPC_VIEW_U, matchedHitList);
                nMatchedHitsV = this->CountHitsByType(TPC_VIEW_V, matchedHitList);
                nMatchedHitsW = this->CountHitsByType(TPC_VIEW_W, matchedHitList);
            }

            pfoPdgVector.push_back(pfoPdg);
            pfoNeutrinoVector.push_back(pfoNeutrinoPdg);
            pfoPxVector.push_back(pfoPx);
            pfoPyVector.push_back(pfoPy); 
            pfoPzVector.push_back(pfoPz); 
            pfoPTotVector.push_back(pfoPTot); 
            pfoVertexVector.push_back(pfoVertex);
            pfoVtxXVector.push_back(pfoVtxX);
            pfoVtxYVector.push_back(pfoVtxY); 
            pfoVtxZVector.push_back(pfoVtxZ);

            // QUICK AND NASTY HACK!!!
            pfoVertexDirVector.push_back(pfoVertexDir);
            pfoVtxXPosVector.push_back(pfoVtxXPos);
            pfoVtxYPosVector.push_back(pfoVtxYPos);
            pfoVtxZPosVector.push_back(pfoVtxZPos);
            pfoVtxXDirVector.push_back(pfoVtxXDir);
            pfoVtxYDirVector.push_back(pfoVtxYDir);
            pfoVtxZDirVector.push_back(pfoVtxZDir);
            // QUICK AND NASTY HACK!!!

            purityVector.push_back((nPfoHits == 0) ? 0 : static_cast<float>(nMatchedHits) / static_cast<float>(nPfoHits));
            completenessVector.push_back((nPfoHits == 0) ? 0 : static_cast<float>(nMatchedHits) / static_cast<float>(nMCHits));
            nMCHitsVector.push_back(nMCHits);
            nPfoHitsVector.push_back(nPfoHits);
            nMatchedHitsVector.push_back(nMatchedHits);

            nMCHitsUVector.push_back(nMCHitsU);
            nPfoHitsUVector.push_back(nPfoHitsU);
            nMatchedHitsUVector.push_back(nMatchedHitsU);

            nMCHitsVVector.push_back(nMCHitsV);
            nPfoHitsVVector.push_back(nPfoHitsV);
            nMatchedHitsVVector.push_back(nMatchedHitsV);

            nMCHitsWVector.push_back(nMCHitsW);
            nPfoHitsWVector.push_back(nPfoHitsW);
            nMatchedHitsWVector.push_back(nMatchedHitsW);
        }
    }
    catch (StatusCodeException &statusCodeException)
    {
        if ((STATUS_CODE_NOT_INITIALIZED != statusCodeException.GetStatusCode()) && (STATUS_CODE_NOT_FOUND != statusCodeException.GetStatusCode()))
            throw statusCodeException;
    }

    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nMCParticlesTotal", nMCParticlesTotal));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nPfosTotal", nPfosTotal));

    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcPdg", &mcPdgVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcNuPdg", &mcNeutrinoVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcPx", &mcPxVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcPy", &mcPyVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcPz", &mcPzVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcPTot", &mcPTotVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcEnergy", &mcEnergyVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcTheta", &mcThetaVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcVtxX", &mcVtxXPosVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcVtxY", &mcVtxYPosVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcVtxZ", &mcVtxZPosVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcEndX", &mcEndXPosVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcEndY", &mcEndYPosVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "mcEndZ", &mcEndZPosVector));

    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoPdg", &pfoPdgVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoNuPdg", &pfoNeutrinoVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoPx", &pfoPxVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoPy", &pfoPyVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoPz", &pfoPzVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoPTot", &pfoPTotVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoVertex", &pfoVertexVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoVtxX", &pfoVtxXVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoVtxY", &pfoVtxYVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoVtxZ", &pfoVtxZVector));

    // QUICK AND NASTY HACK!!!
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoVertexDir", &pfoVertexDirVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoVtxXPos", &pfoVtxXPosVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoVtxYPos", &pfoVtxYPosVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoVtxZPos", &pfoVtxZPosVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoVtxXDir", &pfoVtxXDirVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoVtxYDir", &pfoVtxYDirVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "pfoVtxZDir", &pfoVtxZDirVector));
    // QUICK AND NASTY HACK!!!

    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "completeness", &completenessVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "purity", &purityVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nMCHits", &nMCHitsVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nPfoHits", &nPfoHitsVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nMatchedHits", &nMatchedHitsVector));

    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nMCHitsU", &nMCHitsUVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nPfoHitsU", &nPfoHitsUVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nMatchedHitsU", &nMatchedHitsUVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nMCHitsV", &nMCHitsVVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nPfoHitsV", &nPfoHitsVVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nMatchedHitsV", &nMatchedHitsVVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nMCHitsW", &nMCHitsWVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nPfoHitsW", &nPfoHitsWVector));
    PANDORA_MONITORING_API(SetTreeVariable(this->GetPandora(), m_treeName.c_str(), "nMatchedHitsW", &nMatchedHitsWVector));

    PANDORA_MONITORING_API(FillTree(this->GetPandora(), m_treeName.c_str()));
#endif
    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

void ParticleMonitoringAlgorithm::ExtractNeutrinoDaughters(PfoList &pfoList) const
{
    for (PfoList::iterator iter = pfoList.begin(), iterEnd = pfoList.end(); iter != iterEnd; )
    {
        const ParticleFlowObject *const pPfo(*iter);

        if ((NU_E == std::abs(pPfo->GetParticleId())) || (NU_MU == std::abs(pPfo->GetParticleId())) || (NU_TAU == std::abs(pPfo->GetParticleId())))
        {
            pfoList.insert(pPfo->GetDaughterPfoList().begin(), pPfo->GetDaughterPfoList().end());
            pfoList.erase(iter);
            iter = pfoList.begin();
        }
        else
        {
            ++iter;
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void ParticleMonitoringAlgorithm::GetMCParticleMaps(const MCParticleList *const pMCParticleList, MCRelationMap &mcPrimaryMap) const
{
    for (MCParticleList::const_iterator iter = pMCParticleList->begin(), iterEnd = pMCParticleList->end(); iter != iterEnd; ++iter)
    {
        const MCParticle *const pMCParticle = *iter;

        if (pMCParticle->GetParentList().empty() || LArMCParticleHelper::IsNeutrinoFinalState(pMCParticle))
        {
            if (!LArMCParticleHelper::IsNeutrino(pMCParticle))
                mcPrimaryMap[pMCParticle] = pMCParticle;
        }

        else
        {
            const MCParticle *pParentMCParticle = pMCParticle;

            while (true)
            {
                pParentMCParticle = *(pParentMCParticle->GetParentList().begin());

                if (mcPrimaryMap.find(pParentMCParticle) != mcPrimaryMap.end())
                {
                    mcPrimaryMap[pMCParticle] = mcPrimaryMap[pParentMCParticle];
                    break;
                }
                else if (pParentMCParticle->GetParentList().empty() || LArMCParticleHelper::IsNeutrinoFinalState(pParentMCParticle))
                {
                    mcPrimaryMap[pMCParticle] = pParentMCParticle;
                    break;
                }
            }
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void ParticleMonitoringAlgorithm::GetMCParticleToPfoMatches(const CaloHitList *const pCaloHitList, const PfoList &pfoList,
    const CaloHitToMCMap &mcHitMap, MCToPfoMap &outputPrimaryMap, MCContributionMap &outputContributionMap) const
{
    for (PfoList::const_iterator pIter = pfoList.begin(), pIterEnd = pfoList.end(); pIter != pIterEnd; ++pIter)
    {
        const ParticleFlowObject *const pPfo = *pIter;

        CaloHitList clusterHits;
        this->CollectCaloHits(pPfo, clusterHits);

        MCContributionMap truthContributionMap;

        for (CaloHitList::const_iterator hIter = clusterHits.begin(), hIterEnd = clusterHits.end(); hIter != hIterEnd; ++hIter)
        {
            const CaloHit *const pCaloHit = *hIter;

            if (pCaloHitList->count(pCaloHit) == 0)
                continue;

            CaloHitToMCMap::const_iterator mcIter = mcHitMap.find(pCaloHit);

            if (mcIter == mcHitMap.end())
                continue;

            const MCParticle *const pPrimaryParticle = mcIter->second;
            truthContributionMap[pPrimaryParticle].insert(pCaloHit);
        }

        CaloHitList biggestHitList;
        const MCParticle *biggestContributor = NULL;

        for (MCContributionMap::const_iterator cIter = truthContributionMap.begin(), cIterEnd = truthContributionMap.end(); cIter != cIterEnd; ++cIter)
        {
            if (cIter->second.size() > biggestHitList.size())
            {
                biggestHitList = cIter->second;
                biggestContributor = cIter->first;
            }
        }

        if (biggestContributor)
        {
            MCContributionMap::const_iterator cIter = outputContributionMap.find(biggestContributor);

            if ((outputContributionMap.end() == cIter) || (biggestHitList.size() > cIter->second.size()))
            {
                outputPrimaryMap[biggestContributor] = pPfo;
                outputContributionMap[biggestContributor] = biggestHitList;
            }
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void ParticleMonitoringAlgorithm::GetMCParticleToCaloHitMatches(const CaloHitList *const pCaloHitList, const MCRelationMap &mcPrimaryMap,
    CaloHitToMCMap &mcHitMap, MCContributionMap &mcContributionMap) const
{
    for (CaloHitList::const_iterator iter = pCaloHitList->begin(), iterEnd = pCaloHitList->end(); iter != iterEnd; ++iter)
    {
        try
        {
            const CaloHit *const pCaloHit = *iter;
            const MCParticle *const pHitParticle(MCParticleHelper::GetMainMCParticle(pCaloHit));

            MCRelationMap::const_iterator mcIter = mcPrimaryMap.find(pHitParticle);

            if (mcIter == mcPrimaryMap.end())
                throw StatusCodeException(STATUS_CODE_FAILURE);

            const MCParticle *const pPrimaryParticle = mcIter->second;
            mcContributionMap[pPrimaryParticle].insert(pCaloHit);
            mcHitMap[pCaloHit] = pPrimaryParticle;
        }
        catch (StatusCodeException &statusCodeException)
        {
            if (STATUS_CODE_FAILURE == statusCodeException.GetStatusCode())
                throw statusCodeException;
        }
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void ParticleMonitoringAlgorithm::GetPfoToCaloHitMatches(const CaloHitList *const pCaloHitList, const PfoList &pfoList,
    CaloHitToPfoMap &pfoHitMap, PfoContributionMap &pfoContributionMap) const
{
    for (PfoList::const_iterator pIter = pfoList.begin(), pIterEnd = pfoList.end(); pIter != pIterEnd; ++pIter)
    {
        const ParticleFlowObject *const pPfo = *pIter;
        const ClusterList &pfoClusterList(pPfo->GetClusterList());
        ClusterList clusterList(pfoClusterList.begin(), pfoClusterList.end());

        if (m_useDaughterPfos)
            this->CollectDaughterClusters(pPfo, clusterList);

        CaloHitList pfoHitList;

        for (ClusterList::const_iterator cIter = clusterList.begin(), cIterEnd = clusterList.end(); cIter != cIterEnd; ++cIter)
        {
            const Cluster *const pCluster = *cIter;

            CaloHitList clusterHits;
            pCluster->GetOrderedCaloHitList().GetCaloHitList(clusterHits);
            clusterHits.insert(pCluster->GetIsolatedCaloHitList().begin(), pCluster->GetIsolatedCaloHitList().end());

            for (CaloHitList::const_iterator hIter = clusterHits.begin(), hIterEnd = clusterHits.end(); hIter != hIterEnd; ++hIter)
            {
                const CaloHit *const pCaloHit = *hIter;

                if (pCaloHitList->count(pCaloHit) == 0)
                    continue;

                pfoHitMap[pCaloHit] = pPfo;
                pfoHitList.insert(pCaloHit);
            }
        }

        pfoContributionMap[pPfo] = pfoHitList;
    }
}

//-----------------------------------------------------------------------------------------------------------------------------------------

void ParticleMonitoringAlgorithm::CollectCaloHits(const ParticleFlowObject *const pParentPfo, CaloHitList &caloHitList) const
{
    ClusterList clusterList;
    const ClusterList &pfoClusterList(pParentPfo->GetClusterList());
    clusterList.insert(pfoClusterList.begin(), pfoClusterList.end());

    if (m_useDaughterPfos)
        this->CollectDaughterClusters(pParentPfo, clusterList);

    for (ClusterList::const_iterator cIter = clusterList.begin(), cIterEnd = clusterList.end(); cIter != cIterEnd; ++cIter)
    {
        const Cluster *const pCluster = *cIter;

        if (TPC_3D == LArClusterHelper::GetClusterHitType(pCluster))
            continue;

        pCluster->GetOrderedCaloHitList().GetCaloHitList(caloHitList);
        caloHitList.insert(pCluster->GetIsolatedCaloHitList().begin(), pCluster->GetIsolatedCaloHitList().end());
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

void ParticleMonitoringAlgorithm::CollectDaughterClusters(const ParticleFlowObject *const pParentPfo, ClusterList &clusterList) const
{
    const PfoList &daughterPfoList(pParentPfo->GetDaughterPfoList());
    for (PfoList::const_iterator dIter = daughterPfoList.begin(), dIterEnd = daughterPfoList.end(); dIter != dIterEnd; ++dIter)
    {
        const ParticleFlowObject *const pDaughterPfo = *dIter;
        const ClusterList &pfoClusterList(pDaughterPfo->GetClusterList());

        for (ClusterList::const_iterator cIter = pfoClusterList.begin(), cIterEnd = pfoClusterList.end(); cIter != cIterEnd; ++cIter)
        {
            const Cluster *const pCluster = *cIter;

            if (TPC_3D == LArClusterHelper::GetClusterHitType(pCluster))
                continue;

            if (clusterList.count(pCluster))
                throw StatusCodeException(STATUS_CODE_FAILURE);

            clusterList.insert(pCluster);
        }

        this->CollectDaughterClusters(pDaughterPfo, clusterList);
    }
}

//------------------------------------------------------------------------------------------------------------------------------------------

unsigned int ParticleMonitoringAlgorithm::CountHitsByType(const HitType hitType, const CaloHitList &caloHitList) const
{
    unsigned int nHitsOfSpecifiedType(0);

    for (CaloHitList::const_iterator iter = caloHitList.begin(), iterEnd = caloHitList.end(); iter != iterEnd; ++iter)
    {
        if (hitType == (*iter)->GetHitType())
            ++nHitsOfSpecifiedType;
    }

    return nHitsOfSpecifiedType;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode ParticleMonitoringAlgorithm::ReadSettings(const TiXmlHandle xmlHandle)
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "CaloHitListName", m_caloHitListName));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "MCParticleListName", m_mcParticleListName));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "PfoListName", m_pfoListName));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "OutputTree", m_treeName));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, XmlHelper::ReadValue(xmlHandle, "OutputFile", m_fileName));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "UseDaughterPfos", m_useDaughterPfos));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "ExtractNeutrinoDaughters", m_extractNeutrinoDaughters));

    return STATUS_CODE_SUCCESS;
}

} // namespace lar_content
