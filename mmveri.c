/*****************************************************************************/
/*        Copyright (C) 2004  NORMAN MEGILL  nm at alum.mit.edu              */
/*            License terms:  GNU General Public License                     */
/*****************************************************************************/
/*34567890123456 (79-character line to adjust editor window) 2345678901234567*/

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <setjmp.h>
#include "mmvstr.h"
#include "mmdata.h"
#include "mminou.h"
#include "mmpars.h"
#include "mmveri.h"

/* Global structure used for getting information about a step */
/* If getStep.stepNum is nonzero, we should get info about that step. */
struct getStep_struct getStep = {0, 0, 0, 0, 0,
    NULL_NMBRSTRING, NULL_NMBRSTRING, NULL_PNTRSTRING, NULL_NMBRSTRING,
    NULL_PNTRSTRING};

/* Verify proof of one statement in source file.  Uses wrkProof structure.
   Assumes that parseProof() has just been called for this statement. */
/* Returns 0 if proof is OK; 1 if proof is incomplete (has '?' tokens);
   returns 2 if error found; returns 3 if not severe error
   found; returns 4 if not a $p statement */
char verifyProof(long statemNum)
{

  long stmt; /* Contents of proof string location */
  long i, j, step;
  char type;
  char *fbPtr;
  long tokenLen;
  long numReqHyp;
  char returnFlag = 0;
  nmbrString *nmbrTmpPtr;
  nmbrString *nmbrHypPtr;
  nmbrString *bigSubstSchemeHyp = NULL_NMBRSTRING;
  nmbrString *bigSubstInstHyp = NULL_NMBRSTRING;
  flag unkHypFlag;
  nmbrString *nmbrTmp = NULL_NMBRSTRING; /* Used to force tmp stack dealloc */

  if (statement[statemNum].type != p__) return (4); /* Do nothing if not $p */

  /* Initialize pointers to math strings in RPN stack and vs. statement. */
  /* (Must be initialized, even if severe error, to prevent crashes later.) */
  for (i = 0; i < wrkProof.numSteps; i++) {
    wrkProof.mathStringPtrs[i] = NULL_NMBRSTRING;
  }

  if (wrkProof.errorSeverity > 2) return (wrkProof.errorSeverity);
                                    /* Error too severe to check here */
  wrkProof.RPNStackPtr = 0;

  if (wrkProof.numSteps == 0) return (2);
                        /* Empty proof caused by error found by in parseProof */

  for (step = 0; step < wrkProof.numSteps; step++) {

    stmt = wrkProof.proofString[step];

    /* Handle unknown proof steps */
    if (stmt == -(long)'?') {
      if (returnFlag < 1) returnFlag = 1;
                                      /* Flag that proof is partially unknown */
      /* Treat "?" like a hypothesis - push stack and continue */
      wrkProof.RPNStack[wrkProof.RPNStackPtr] = step;

      wrkProof.RPNStackPtr++;
      /* Leave the step's math string empty and continue */
      continue;
    }

    /* See if the proof token is a local label ref. */
    if (stmt < 0) {
      /* It's a local label reference */
      if (stmt > -1000) bug(2101);
      i = -1000 - stmt; /* Get the step number it refers to */

      /* Push the stack */
      wrkProof.RPNStack[wrkProof.RPNStackPtr] = step;
      wrkProof.RPNStackPtr++;

      /* Assign a math string to the step (must not be deallocated by
         cleanWrkProof()!) */
      wrkProof.mathStringPtrs[step] =
          wrkProof.mathStringPtrs[i];

      continue;
    }

    type = statement[stmt].type;

    /* See if the proof token is a hypothesis */
    if (type == e__ || type == f__) {
      /* It's a hypothesis reference */

      /* Push the stack */
      wrkProof.RPNStack[wrkProof.RPNStackPtr] = step;
      wrkProof.RPNStackPtr++;

      /* Assign a math string to the step (must not be deallocated by
         cleanWrkProof()!) */
      wrkProof.mathStringPtrs[step] =
          statement[stmt].mathString;

      continue;
    }

    /* The proof token must be an assertion */
    if (type != a__ && type != p__) bug(2102);

    /* It's an valid assertion. */
    numReqHyp = statement[stmt].numReqHyp;
    nmbrHypPtr = statement[stmt].reqHypList;

    /* Assemble the hypotheses into two big math strings for unification */
    /* Use a "dummy" token, the top of mathTokens array, to separate them. */
    /* This is already done by the source parsing routines:
    mathToken[mathTokens].tokenType = (char)con__;
    mathToken[mathTokens].tokenName = "$|$"; */ /* Don't deallocate! */

    nmbrLet(&bigSubstSchemeHyp, nmbrAddElement(NULL_NMBRSTRING, mathTokens));
    nmbrLet(&bigSubstInstHyp, nmbrAddElement(NULL_NMBRSTRING, mathTokens));
    unkHypFlag = 0; /* Flag that there are unknown hypotheses */
    j = 0;
    for (i = wrkProof.RPNStackPtr - numReqHyp; i < wrkProof.RPNStackPtr; i++) {
      nmbrTmpPtr = wrkProof.mathStringPtrs[
          wrkProof.RPNStack[i]];
      if (nmbrTmpPtr[0] == -1) { /* If length is zero, hyp is unknown */
        unkHypFlag = 1;
        /* Assign scheme to empty nmbrString so it will always match instance */
        nmbrLet(&bigSubstSchemeHyp,
            nmbrCat(bigSubstSchemeHyp,
            nmbrAddElement(nmbrTmpPtr, mathTokens), NULL));
      } else {
        nmbrLet(&bigSubstSchemeHyp,
            nmbrCat(bigSubstSchemeHyp,
            nmbrAddElement(statement[nmbrHypPtr[j]].mathString,
            mathTokens), NULL));
      }
      nmbrLet(&bigSubstInstHyp,
          nmbrCat(bigSubstInstHyp,
          nmbrAddElement(nmbrTmpPtr, mathTokens), NULL));
      j++;

      /* Get information about the step if requested */
      if (getStep.stepNum) { /* If non-zero, step info is requested */
        if (wrkProof.RPNStack[i] == getStep.stepNum - 1) {
          /* Get parent of target if this is one of its hyp's */
          getStep.targetParentStep = step + 1;
          getStep.targetParentStmt = stmt;
        }
        if (step == getStep.stepNum - 1) {
          /* Add to source hypothesis list */
          nmbrLet(&getStep.sourceHyps, nmbrAddElement(getStep.sourceHyps,
              wrkProof.RPNStack[i]));
        }
      } /* End of if (getStep.stepNum) */

    }

/*E*/if(db7)printLongLine(cat("step ", str(step+1), " sch ",
/*E*/    nmbrCvtMToVString(bigSubstSchemeHyp), NULL), "", " ");
/*E*/if(db7)printLongLine(cat("step ", str(step+1), " ins ",
/*E*/    nmbrCvtMToVString(bigSubstInstHyp), NULL), "", " ");
    /* Unify the hypotheses of the scheme with their instances and assign
       the variables of the scheme.  If some of the hypotheses are unknown
       (due to proof being debugged or previous error) we will try to unify
       anyway; if the result is unique, we will use it. */
    nmbrTmpPtr = assignVar(bigSubstSchemeHyp,
        bigSubstInstHyp, stmt, statemNum, step, unkHypFlag);
/*E*/if(db7)printLongLine(cat("step ", str(step+1), " res ",
/*E*/    nmbrCvtMToVString(nmbrTmpPtr), NULL), "", " ");

    /* Deallocate stack built up if there are many $d violations */
    nmbrLet(&nmbrTmp, NULL_NMBRSTRING);

    /* Assign the substituted assertion (must be deallocated by
         cleanWrkProof()!) */
    wrkProof.mathStringPtrs[step] = nmbrTmpPtr;
    if (nmbrTmpPtr[0] == -1) {
      if (!unkHypFlag) {
        returnFlag = 2; /* An error occurred (assignVar printed it) */
      }
    }


    /* Pop the stack */
    wrkProof.RPNStackPtr = wrkProof.RPNStackPtr - numReqHyp;
    wrkProof.RPNStack[wrkProof.RPNStackPtr] = step;
    wrkProof.RPNStackPtr++;

  } /* Next step */

  /* If there was a stack error, the verifier should have never been
     called. */
  if (wrkProof.RPNStackPtr != 1) bug(2108);

  /* See if the result matches the statement to be proved */
  if (returnFlag == 0) {
    if (!nmbrEq(statement[statemNum].mathString,
        wrkProof.mathStringPtrs[wrkProof.numSteps - 1])) {
      if (!wrkProof.errorCount) {
        fbPtr = wrkProof.stepSrcPtrPntr[wrkProof.numSteps - 1];
        tokenLen = wrkProof.stepSrcPtrNmbr[wrkProof.numSteps - 1];
        /*??? Make sure suggested commands are correct. */
        sourceError(fbPtr, tokenLen, statemNum, cat(
            "The result of the proof (step ",str(wrkProof.numSteps),
            ") does not match the statement being proved.  The result is \"",
            nmbrCvtMToVString(
            wrkProof.mathStringPtrs[wrkProof.numSteps - 1]),
            "\" but the statement is \"",
            nmbrCvtMToVString(statement[statemNum].mathString),
            "\".  Type \"SHOW PROOF ",statement[statemNum].labelName,
            "\" to see the proof attempt.",NULL));
      }
      wrkProof.errorCount++;
    }
  }

  nmbrLet(&bigSubstSchemeHyp, NULL_NMBRSTRING);
  nmbrLet(&bigSubstInstHyp, NULL_NMBRSTRING);

  return (returnFlag);

}




/* assignVar() finds an assignment to substScheme variables that match
   the assumptions specified in the reason string */
nmbrString *assignVar(nmbrString *bigSubstSchemeAss,
  nmbrString *bigSubstInstAss, long substScheme,
  /* For error messages: */
  long statementNum, long step, flag unkHypFlag)
{
  nmbrString *bigSubstSchemeVars = NULL_NMBRSTRING;
  nmbrString *substSchemeFrstVarOcc = NULL_NMBRSTRING;
  nmbrString *varAssLen = NULL_NMBRSTRING;
  nmbrString *substInstFrstVarOcc = NULL_NMBRSTRING;
  nmbrString *result = NULL_NMBRSTRING; /* value returned */
  long bigSubstSchemeLen,bigSubstInstLen,bigSubstSchemeVarLen,substSchemeLen,
      resultLen;
  long i,v,v1,p,q,tokenNum;
  flag breakFlag, contFlag;
  vstring tmpStr = "";
  vstring tmpStr2 = "";
  flag ambiguityCheckFlag = 0;
  nmbrString *saveResult = NULL_NMBRSTRING;

  /* Variables for disjoint variable ($d) check */
  nmbrString *nmbrTmpPtrAS;
  nmbrString *nmbrTmpPtrBS;
  nmbrString *nmbrTmpPtrAIR;
  nmbrString *nmbrTmpPtrBIR;
  nmbrString *nmbrTmpPtrAIO;
  nmbrString *nmbrTmpPtrBIO;
  long dLen, pos, substAPos, substALen, instAPos, substBPos, substBLen,
      instBPos, a, b, aToken, bToken, aToken2, bToken2, dILenR, dILenO,
      optStart, reqStart;
  flag foundFlag;

  /* Variables for getting step info */
  long j,k,m;
  long numReqHyp;
  nmbrString *nmbrHypPtr;

  nmbrString *nmbrTmp = NULL_NMBRSTRING; /* Used to force tmp stack dealloc */

  long nmbrSaveTempAllocStack;

  /* Initialization to avoid compiler warnings (should not be theoretically
     necessary) */
  dILenR = 0;
  dILenO = 0;
  optStart = 0;
  reqStart = 0;
  nmbrTmpPtrAIR = NULL_NMBRSTRING;
  nmbrTmpPtrBIR = NULL_NMBRSTRING;
  nmbrTmpPtrAIO = NULL_NMBRSTRING;
  nmbrTmpPtrBIO = NULL_NMBRSTRING;

  nmbrSaveTempAllocStack = nmbrStartTempAllocStack;
  nmbrStartTempAllocStack = nmbrTempAllocStackTop; /* For nmbrLet() stack cleanup*/

  bigSubstSchemeLen = nmbrLen(bigSubstSchemeAss);
  bigSubstInstLen = nmbrLen(bigSubstInstAss);
  nmbrLet(&bigSubstSchemeVars,nmbrExtractVars(bigSubstSchemeAss));
  bigSubstSchemeVarLen = nmbrLen(bigSubstSchemeVars);

  /* If there are no variables in the hypotheses, there won't be any in the
     assertion (unless there was a previously detected error).  In this case,
     the unification is just the assertion itself. */
  if (!bigSubstSchemeVarLen) {
    if (!nmbrLen(statement[substScheme].reqVarList)) {
      nmbrLet(&result,statement[substScheme].mathString);
    } else {
      /* There must have been a previous error; let result remain empty */
      if (!unkHypFlag) bug(2109);
    }
    goto returnPoint;
  }

  /* Allocate nmbrStrings used only to hold extra data for bigSubstSchemeAss;
     don't use further nmbrString functions on them! */
  /* substSchemeFrstVarOcc[] is the 1st occurrence of the variable
         in bigSubstSchemeAss */
  /* varAssLen[] is the length of the
         assignment to the variable */
  /* substInstFrstVarOcc[] is the 1st occurrence of the variable
         in bigSubstInstAss */
  nmbrLet(&substSchemeFrstVarOcc,nmbrSpace(bigSubstSchemeVarLen));
  nmbrLet(&varAssLen,substSchemeFrstVarOcc);
  nmbrLet(&substInstFrstVarOcc,substSchemeFrstVarOcc);

  if (bigSubstSchemeVarLen != nmbrLen(statement[substScheme].reqVarList)) {
    if (unkHypFlag) {
      /* If there are unknown hypotheses and all variables aren't present,
         give up here */
      goto returnPoint;
    } else {
      /* Actually, this could happen if there was a previous error,
         which would have already been reported. */
      if (!wrkProof.errorCount) bug(2103); /* There must have been an error */
      goto returnPoint;
    }
  }

  for (i = 0; i < bigSubstSchemeVarLen; i++) {
    substSchemeFrstVarOcc[i] = -1; /* Initialize */
    /* (varAssLen[], substInstFrstVarOcc[], are
       all initialized to 0 by nmbrSpace().) */
  }

  /* Use the .tmp field of mathToken[]. to hold position of variable in
     bigSubstSchemeVars for quicker lookup */
  for (i = 0; i < bigSubstSchemeVarLen; i++) {
    mathToken[bigSubstSchemeVars[i]].tmp = i;
  }

  /* Scan bigSubstSchemeAss to get substSchemeFrstVarOcc[] (1st var
     occurrence) */
  for (i = 0; i < bigSubstSchemeLen; i++) {
    if (mathToken[bigSubstSchemeAss[i]].tokenType ==
        (char)var__) {
      if (substSchemeFrstVarOcc[mathToken[bigSubstSchemeAss[
          i]].tmp] == -1) {
        substSchemeFrstVarOcc[mathToken[bigSubstSchemeAss[
            i]].tmp] = i;
      }
    }
  }

  /* Do the scan */
  v = -1; /* Position in bigSubstSchemeVars */
  p = 0; /* Position in bigSubstSchemeAss */
  q = 0; /* Position in bigSubstInstAss */
 ambiguityCheck: /* Re-entry point to see if unification is unique */
  while (p != bigSubstSchemeLen-1 || q != bigSubstInstLen-1) {
/*E*/if(db7&&v>=0)printLongLine(cat("p ", str(p), " q ", str(q), " VAR ",str(v),
/*E*/    " ASSIGNED ", nmbrCvtMToVString(
/*E*/    nmbrMid(bigSubstInstAss,substInstFrstVarOcc[v]+1,
/*E*/    varAssLen[v])), NULL), "", " ");
/*E*/if(db7)nmbrLet(&bigSubstInstAss,bigSubstInstAss);
/*E*/if(db7){print2("Enter scan: v=%ld,p=%ld,q=%ld\n",v,p,q); let(&tmpStr,"");}
    tokenNum = bigSubstSchemeAss[p];
    if (mathToken[tokenNum].tokenType == (char)con__) {
      /* Constants must match in both substScheme and definiendum assumptions */
      if (tokenNum == bigSubstInstAss[q]) {
        p++;
        q++;
/*E*/if(db7)print2(" Exit, c ok: v=%ld,p=%ld,q=%ld\n",v,p,q);
        continue;
      } else {
        /* Backtrack to last variable assigned and add 1 to its length */
        breakFlag = 0;
        contFlag = 1;
        while (contFlag) {
          if (v < 0) {
            breakFlag = 1;
            break; /* Error - possibilities exhausted */
          }
          varAssLen[v]++;
          p = substSchemeFrstVarOcc[v] + 1;
          q = substInstFrstVarOcc[v] + varAssLen[v];
          contFlag = 0;
          if (bigSubstInstAss[q-1] == mathTokens) {
            /* It ran into the dummy token separating the assumptions.
               A variable cannot be assigned this dummy token.  Therefore,
               we must pop back a variable.  (This test speeds up
               the program; theoretically, it is not needed.) */
/*E*/if(db7)print2("GOT TO DUMMY TOKEN1\n");
            v--;
            contFlag = 1;
            continue;
          }
          if (q >= bigSubstInstLen) {
            /* It overflowed the end of bigSubstInstAss; pop back a variable */
            v--;
            contFlag = 1;
            bug(2104); /* Should be trapped above */
          }
        } /* end while */
        if (breakFlag) {
/*E*/if(db7)print2(" Exit, c bktrk bad: v=%ld,p=%ld,q=%ld\n",v,p,q);
          break;
        }
/*E*/if(db7)print2(" Exit, c bktrk ok: v=%ld,p=%ld,q=%ld\n",v,p,q);
      }
    } else {
      /* It's a variable.  If its the first occurrence, init length to 0 */
      v1 = mathToken[tokenNum].tmp;
      if (v1 > v) {
        if (v1 != v + 1) bug(2105);
        v = v1;
        varAssLen[v] = 0; /* variable length */
        substInstFrstVarOcc[v] = q;   /* variable start in bigSubstInstAss */
        p++;
/*E*/if(db7)print2(" Exit, v new: v=%ld,p=%ld,q=%ld\n",v,p,q);
        continue;
      } else { /* It's not the first occurrence; check that it matches */
        breakFlag = 0;
        for (i = 0; i < varAssLen[v1]; i++) {
          if (q + i >= bigSubstInstLen) {
            /* It overflowed the end of bigSubstInstAss */
            breakFlag = 1;
            break;
          }
          if (bigSubstInstAss[substInstFrstVarOcc[v1] + i] !=
              bigSubstInstAss[q + i]) {
            /* The variable assignment mismatched */
            breakFlag = 1;
            break;
          }
        }
        if (breakFlag) {
          /* Backtrack to last variable assigned and add 1 to its length */
          breakFlag = 0;
          contFlag = 1;
          while (contFlag) {
            if (v < 0) {
              breakFlag = 1;
              break; /* Error - possibilities exhausted */
            }
            varAssLen[v]++;
            p = substSchemeFrstVarOcc[v] + 1;
            q = substInstFrstVarOcc[v] + varAssLen[v];
            contFlag = 0;
            if (bigSubstInstAss[q-1] == mathTokens) {
              /* It ran into the dummy token separating the assumptions.
                 A variable cannot be assigned this dummy token.  Therefore,
                 we must pop back a variable.  (This test speeds up
                 the program; theoretically, it is not needed.) */
/*E*/if(db7)print2("GOT TO DUMMY TOKEN\n");
              v--;
              contFlag = 1;
            }
            if (q >= bigSubstInstLen) {
              /* It overflowed the end of bigSubstInstAss; pop back a variable */
              v--;
              contFlag = 1;
              bug(2106); /* Should be trapped above */
            }
          }
          if (breakFlag) {
/*E*/if(db7)print2(" Exit, vold bck bad: v=%ld,p=%ld,q=%ld\n",v,p,q);
            break;
          }
/*E*/if(db7)print2(" Exit, vold bck ok: v=%ld,p=%ld,q=%ld\n",v,p,q);
          continue;
        } else {
          p++;
          q = q + varAssLen[v1];
/*E*/if(db7)print2(" Exit, vold ok: v=%ld,p=%ld,q=%ld\n",v,p,q);
          continue;
        }
      } /* end if first occurrence */
    } /* end if constant */
  } /* end while */

/*E*/if(db7)printLongLine(cat("BIGVR ", nmbrCvtMToVString(bigSubstSchemeVars),
/*E*/    NULL), "", " ");
/*E*/if(db7)print2(
/*E*/"p=%ld,bigSubstSchemeLen=%ld;q=%ld,bigSubstInstLen=%ld;v=%ld,bigSubstSchemeVarLen=%ld\n",
/*E*/  p,bigSubstSchemeLen,q,bigSubstInstLen,v,bigSubstSchemeVarLen);
  /* See if the assignment completed normally */
  if (v == -1) {
    if (ambiguityCheckFlag) {
      /* This is what we wanted to see -- no further unification possible */
      goto returnPoint;
    }
    if (!wrkProof.errorCount) {
      let(&tmpStr, "");
      j = statement[substScheme].numReqHyp;
      for (i = 0; i < j; i++) {
        k = wrkProof.RPNStack[wrkProof.RPNStackPtr - j + i]; /* Step */
        let(&tmpStr2, nmbrCvtMToVString(wrkProof.mathStringPtrs[k]));
        if (tmpStr2[0] == 0) let(&tmpStr2,
            "? (Unknown step or previous error; unification ignored)");
        let(&tmpStr, cat(tmpStr, "\n  Hypothesis ", str(i + 1), ":  ",
            nmbrCvtMToVString(
                statement[statement[substScheme].reqHypList[i]].mathString),
            "\n  Step ", str(k + 1),
            ":  ", tmpStr2, NULL));
      } /* Next i */
      /* tmpStr = shortDumpRPNStack(); */ /* Old version */
      sourceError(wrkProof.stepSrcPtrPntr[step],
          wrkProof.stepSrcPtrNmbr[step],
          statementNum, cat(
          "The hypotheses of statement \"", statement[substScheme].labelName,
          "\" at proof step ", str(step + 1),
          " cannot be unified.", tmpStr, NULL));
      /* sourceError(wrkProof.stepSrcPtrPntr[step],
          wrkProof.stepSrcPtrNmbr[step],
          statementNum, cat(
          "The hypotheses of the statement at proof step ",
          str(step + 1),
          " cannot be unified.  The statement \"",
          statement[substScheme].labelName,
          "\" requires ",
          str(statement[substScheme].numReqHyp),
          " hypotheses.  The ",tmpStr,
          ".  Type \"SHOW PROOF ",statement[statementNum].labelName,
          "\" to see the proof attempt.",NULL)); */ /* Old version */
      let(&tmpStr, "");
      let(&tmpStr2, "");
    }
    wrkProof.errorCount++;
    goto returnPoint;
  }
  if (p != bigSubstSchemeLen - 1 || q != bigSubstInstLen - 1
      || v != bigSubstSchemeVarLen - 1) bug(2107);

  /* If a second unification was possible, save the first result for the
     error message */
  if (ambiguityCheckFlag) {
    if (unkHypFlag) {
      /* If a hypothesis was unknown, the fact that the unification is ambiguous
         doesn't matter, so just return with an empty (unknown) answer. */
      nmbrLet(&result,NULL_NMBRSTRING);
      goto returnPoint;
    }
    nmbrLet(&saveResult, result);
    nmbrLet(&result, NULL_NMBRSTRING);
  }


  /***** Get step information if requested *****/
  if (!ambiguityCheckFlag) { /* This is the real (first) unification */
    if (getStep.stepNum) {
      /* See if this step is the requested step; if so get source substitutions */
      if (step + 1 == getStep.stepNum) {
        nmbrLet(&getStep.sourceSubstsNmbr, nmbrExtractVars(
            statement[substScheme].mathString));
        k = nmbrLen(getStep.sourceSubstsNmbr);
        pntrLet(&getStep.sourceSubstsPntr,
            pntrNSpace(k));
        for (m = 0; m < k; m++) {
          pos = mathToken[getStep.sourceSubstsNmbr[m]].tmp; /* Subst pos */
          nmbrLet((nmbrString **)(&getStep.sourceSubstsPntr[m]),
              nmbrMid(bigSubstInstAss,
              substInstFrstVarOcc[pos] + 1, /* Subst pos */
              varAssLen[pos]) /* Subst length */ );
        }
      }
      /* See if this step is a target hyp; if so get target substitutions */
      j = 0;
      numReqHyp = statement[substScheme].numReqHyp;
      nmbrHypPtr = statement[substScheme].reqHypList;
      for (i = wrkProof.RPNStackPtr - numReqHyp; i < wrkProof.RPNStackPtr; i++) {
        if (wrkProof.RPNStack[i] == getStep.stepNum - 1) {
          /* This is parent of target; get hyp's variable substitutions */
          nmbrLet(&getStep.targetSubstsNmbr, nmbrExtractVars(
              statement[nmbrHypPtr[j]].mathString));
          k = nmbrLen(getStep.targetSubstsNmbr);
          pntrLet(&getStep.targetSubstsPntr, pntrNSpace(k));
          for (m = 0; m < k; m++) {
            pos = mathToken[getStep.targetSubstsNmbr[m]].tmp;
                                                     /* Substitution position */
            nmbrLet((nmbrString **)(&getStep.targetSubstsPntr[m]),
                nmbrMid(bigSubstInstAss,
                substInstFrstVarOcc[pos] + 1, /* Subst pos */
                varAssLen[pos]) /* Subst length */ );
          } /* Next m */
        } /* End if (wrkProof.RPNStack[i] == getStep.stepNum - 1) */
        j++;
      } /* Next i */
    } /* End if (getStep.stepNum) */
  } /* End if (!ambiguityCheckFlag) */
  /***** End of getting step information *****/


  /***** Check for $d violations *****/
  if (!ambiguityCheckFlag) { /* This is the real (first) unification */
    nmbrTmpPtrAS = statement[substScheme].reqDisjVarsA;
    nmbrTmpPtrBS = statement[substScheme].reqDisjVarsB;
    dLen = nmbrLen(nmbrTmpPtrAS); /* Number of disjoint variable pairs */
    if (dLen) { /* There is a disjoint variable requirement */
      /* (Speedup) Save pointers and lengths for statement being proved */
      nmbrTmpPtrAIR = statement[statementNum].reqDisjVarsA;
      nmbrTmpPtrBIR = statement[statementNum].reqDisjVarsB;
      dILenR = nmbrLen(nmbrTmpPtrAIR); /* Number of disj hypotheses */
      nmbrTmpPtrAIO = statement[statementNum].optDisjVarsA;
      nmbrTmpPtrBIO = statement[statementNum].optDisjVarsB;
      dILenO = nmbrLen(nmbrTmpPtrAIO); /* Number of disj hypotheses */
    }
    for (pos = 0; pos < dLen; pos++) { /* Scan the disj var pairs */
      substAPos = mathToken[nmbrTmpPtrAS[pos]].tmp;
      substALen = varAssLen[substAPos];
      instAPos = substInstFrstVarOcc[substAPos];
      substBPos = mathToken[nmbrTmpPtrBS[pos]].tmp;
      substBLen = varAssLen[substBPos];
      instBPos = substInstFrstVarOcc[substBPos];
      for (a = 0; a < substALen; a++) { /* Scan subst of 1st var in disj pair */
        aToken = bigSubstInstAss[instAPos + a];
        if (mathToken[aToken].tokenType == (char)con__) continue; /* Ignore */

        /* Speed up:  find the 1st occurrence of aToken in the disjoint variable
           list of the statement being proved. */
        /* To bypass speedup, we would do this:
              reqStart = 0;
              optStart = 0; */
        /* First, see if the variable is in the required list. */
        foundFlag = 0;
        for (i = 0; i < dILenR; i++) {
          if (nmbrTmpPtrAIR[i] == aToken
              || nmbrTmpPtrBIR[i] == aToken) {
            foundFlag = 1;
            reqStart = i;
            break;
          }
        }
        /* If not, see if it is in the optional list. */
        if (!foundFlag) {
          reqStart = dILenR; /* Force skipping required scan */
          foundFlag = 0;
          for (i = 0; i < dILenO; i++) {
            if (nmbrTmpPtrAIO[i] == aToken
                || nmbrTmpPtrBIO[i] == aToken) {
              foundFlag = 1;
              optStart = i;
              break;
            }
          }
          if (!foundFlag) optStart = dILenO; /* Force skipping optional scan */
        } else {
          optStart = 0;
        } /* (End if (!foundFlag)) */
        /* (End of speedup section) */

        for (b = 0; b < substBLen; b++) { /* Scan subst of 2nd var in pair */
          bToken = bigSubstInstAss[instBPos + b];
          if (mathToken[bToken].tokenType == (char)con__) continue; /* Ignore */
          if (aToken == bToken) {
            if (!wrkProof.errorCount) { /* No previous errors in this proof */
              sourceError(wrkProof.stepSrcPtrPntr[step], /* source ptr */
                  wrkProof.stepSrcPtrNmbr[step], /* size of token */
                  statementNum, cat(
                  "There is a disjoint variable ($d) violation at proof step ",
                  str(step + 1),".  Assertion \"",
                  statement[substScheme].labelName,
                  "\" requires that variables \"",
                  mathToken[nmbrTmpPtrAS[pos]].tokenName,
                  "\" and \"",
                  mathToken[nmbrTmpPtrBS[pos]].tokenName,
                  "\" be disjoint.  But \"",
                  mathToken[nmbrTmpPtrAS[pos]].tokenName,
                  "\" was substituted with \"",
                  nmbrCvtMToVString(nmbrMid(bigSubstInstAss,instAPos + 1,
                      substALen)),
                  "\" and \"",
                  mathToken[nmbrTmpPtrBS[pos]].tokenName,
                  "\" was substituted with \"",
                  nmbrCvtMToVString(nmbrMid(bigSubstInstAss,instBPos + 1,
                      substBLen)),
                  "\".  These substitutions have variable \"",
                  mathToken[aToken].tokenName,
                  "\" in common.",
                  NULL));
              let(&tmpStr, ""); /* Force tmp string stack dealloc */
              nmbrLet(&nmbrTmp,NULL_NMBRSTRING); /* Force tmp stack dealloc */
            } /* (End if (!wrkProof.errorCount) ) */
          } else { /* aToken != bToken */
            /* The variables are different.  We're still not done though:  We
               must make sure that the $d's of the statement being proved
               guarantee that they will be disjoint. */
            /*???Future:  use bsearch for speedup?  Must modify main READ
               parsing to produce sorted disj var lists; this would slow down
               the main READ. */
            /* Make sure that the variables are in the right order for lookup.*/
            if (aToken > bToken) {
              aToken2 = bToken;
              bToken2 = aToken;
            } else {
              aToken2 = aToken;
              bToken2 = bToken;
            }
            /* Scan the required disjoint variable hypotheses to see if they're
               in it. */
            /* First, see if both variables are in the required list. */
            foundFlag = 0;
            for (i = reqStart; i < dILenR; i++) {
              if (nmbrTmpPtrAIR[i] == aToken2) {
                if (nmbrTmpPtrBIR[i] == bToken2) {
                  foundFlag = 1;
                  break;
                }
              }
            }
            /* If not, see if they are in the optional list. */
            if (!foundFlag) {
              foundFlag = 0;
              for (i = optStart; i < dILenO; i++) {
                if (nmbrTmpPtrAIO[i] == aToken2) {
                  if (nmbrTmpPtrBIO[i] == bToken2) {
                    foundFlag = 1;
                    break;
                  }
                }
              }
            } /* (End if (!foundFlag)) */
            /* If they were in neither place, we have a violation. */
            if (!foundFlag) {
              if (!wrkProof.errorCount) { /* No previous errors in this proof */
                sourceError(wrkProof.stepSrcPtrPntr[step], /* source */
                    wrkProof.stepSrcPtrNmbr[step], /* size of token */
                    statementNum, cat(
                   "There is a disjoint variable ($d) violation at proof step ",
                    str(step + 1), ".  Assertion \"",
                    statement[substScheme].labelName,
                    "\" requires that variables \"",
                    mathToken[nmbrTmpPtrAS[pos]].tokenName,
                    "\" and \"",
                    mathToken[nmbrTmpPtrBS[pos]].tokenName,
                    "\" be disjoint.  But \"",
                    mathToken[nmbrTmpPtrAS[pos]].tokenName,
                    "\" was substituted with \"",
                    nmbrCvtMToVString(nmbrMid(bigSubstInstAss, instAPos + 1,
                        substALen)),
                    "\" and \"",
                    mathToken[nmbrTmpPtrBS[pos]].tokenName,
                    "\" was substituted with \"",
                    nmbrCvtMToVString(nmbrMid(bigSubstInstAss, instBPos + 1,
                        substBLen)),
                    "\".", NULL));
                /* Put missing $d requirement in new line so grep can find
                   them easily in log file */
                printLongLine(cat("Variables \"",
                    mathToken[aToken].tokenName, "\" and \"",
                    mathToken[bToken].tokenName,
                    "\" do not have a disjoint variable requirement in the ",
                    "assertion being proved, \"",
                    statement[statementNum].labelName,
                    "\".", NULL), "", " ");
                let(&tmpStr, ""); /* Force tmp string stack dealloc */
                nmbrLet(&nmbrTmp,NULL_NMBRSTRING); /* Force tmp stack dealloc */
              } /* (End if (!wrkProof.errorCount) ) */
            } /* (End if (!foundFlag)) */
          } /* (End if (aToken == bToken)) */
        } /* (Next b) */
      } /* (Next a) */
    } /* (Next pos) */
  } /* (End if (!ambiguityCheck)) */
  /***** (End of $d violation check) *****/

  /* Assemble the final result */
  substSchemeLen = nmbrLen(statement[substScheme].mathString);
  /* Calculate the length of the final result */
  q = 0;
  for (p = 0; p < substSchemeLen; p++) {
    tokenNum = statement[substScheme].mathString[p];
    if (mathToken[tokenNum].tokenType == (char)con__) {
      q++;
    } else {
      q = q + varAssLen[mathToken[tokenNum].tmp];
    }
  }
  /* Allocate space for the final result */
  resultLen = q;
  nmbrLet(&result,nmbrSpace(resultLen));
  /* Assign the final result */
  q = 0;
  for (p = 0; p < substSchemeLen; p++) {
    tokenNum = statement[substScheme].mathString[p];
    if (mathToken[tokenNum].tokenType == (char)con__) {
      result[q] = tokenNum;
      q++;
    } else {
      for (i = 0; i < varAssLen[mathToken[tokenNum].tmp]; i++){
        result[q + i] = bigSubstInstAss[i +
            substInstFrstVarOcc[mathToken[tokenNum].tmp]];
      }
      q = q + i;
    }
  }
/*E*/if(db7)printLongLine(cat("result ", nmbrCvtMToVString(result), NULL),""," ");

  if (ambiguityCheckFlag) {
    if (!wrkProof.errorCount) {
      /*??? Make sure suggested commands are correct. */
      sourceError(wrkProof.stepSrcPtrPntr[step],
          wrkProof.stepSrcPtrNmbr[step],
          statementNum, cat(
          "The unification with the hypotheses of the statement at proof step ",
          str(step + 1),
          " is not unique.  Two possible results at this step are \"",
          nmbrCvtMToVString(saveResult),
          "\" and \"",nmbrCvtMToVString(result),
          "\".  Type \"SHOW PROOF ",statement[statementNum].labelName,
          "\" to see the proof attempt.",NULL));
    }
    wrkProof.errorCount++;
    goto returnPoint;
  } else {

    /* Prepare to see if the unification is unique */
    while (1) {
      v--;
      if (v < 0) {
        goto returnPoint; /* It's unique */
      }
      varAssLen[v]++;
      p = substSchemeFrstVarOcc[v] + 1;
      q = substInstFrstVarOcc[v] + varAssLen[v];
      if (bigSubstInstAss[q - 1] != mathTokens) break;
      if (q >= bigSubstInstLen) bug(2110);
    }
    ambiguityCheckFlag = 1;
    goto ambiguityCheck;
  }


 returnPoint:

  /* Free up all allocated nmbrString space */
  for (i = 0; i < bigSubstSchemeVarLen; i++) {
    /* Make the data-holding structures legal nmbrStrings before nmbrLet() */
    /*???Make more efficient by deallocating directly*/
    substSchemeFrstVarOcc[i] = 0;
    varAssLen[i] = 0;
    substInstFrstVarOcc[i] = 0;
  }
  nmbrLet(&bigSubstSchemeVars,NULL_NMBRSTRING);
  nmbrLet(&substSchemeFrstVarOcc,NULL_NMBRSTRING);
  nmbrLet(&varAssLen,NULL_NMBRSTRING);
  nmbrLet(&substInstFrstVarOcc,NULL_NMBRSTRING);
  nmbrLet(&saveResult,NULL_NMBRSTRING);

  nmbrStartTempAllocStack = nmbrSaveTempAllocStack;
  return(result);

}


/* Deallocate the math symbol strings assigned in wrkProof structure during
   proof verification.  This should be called after verifyProof() and after the
   math symbol strings have been used for proof printouts, etc. */
/* Note that this does NOT free the other allocations in wrkProof.  The
   ERASE command will do this. */
void cleanWrkProof(void) {

  long step;
  char type;

  for (step = 0; step < wrkProof.numSteps; step++) {
    if (wrkProof.proofString[step] > 0) {
      type = statement[wrkProof.proofString[step]].type;
      if (type == a__ || type == p__) {
        /* Allocation was only done if: (1) it's not a local label reference
           and (2) it's not a hypothesis.  In this case, deallocate. */
        nmbrLet((nmbrString **)(&wrkProof.mathStringPtrs[step]),
            NULL_NMBRSTRING);
      }
    }
  }

}
