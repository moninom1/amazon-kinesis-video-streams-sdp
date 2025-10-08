/* Standard includes. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* Interface includes. */
#include "sdp_deserializer.h"

/* Max value that fits in a size_t type. */
#define sdpSIZE_MAX    ( ~( ( size_t ) 0 ) )

/* Check if adding a and b will result in overflow. */
#define sdpADD_WILL_OVERFLOW( a, b )    ( ( a ) > ( sdpSIZE_MAX - ( b ) ) )

SdpResult_t SdpDeserializer_Init( SdpDeserializerContext_t * pCtx,
                                  const char * pSdpMessage,
                                  size_t sdpMessageLength )
{
    SdpResult_t result = SDP_RESULT_OK;

    if( ( pCtx == NULL ) ||
        ( pSdpMessage == NULL ) ||
        ( sdpMessageLength == 0 ) )
    {
        result = SDP_RESULT_BAD_PARAM;
    }

    if( result == SDP_RESULT_OK )
    {
        pCtx->pStart = pSdpMessage;
        pCtx->totalLength = sdpMessageLength;
        pCtx->currentIndex = 0;
    }

    return result;
}
/*-----------------------------------------------------------*/

SdpResult_t SdpDeserializer_GetNext( SdpDeserializerContext_t * pCtx,
                                     uint8_t * pType,
                                     const char ** pValue,
                                     size_t * pValueLength )
{
    size_t i, remainingLength, valueLength;
    SdpResult_t result = SDP_RESULT_OK;

    if( ( pCtx == NULL ) ||
        ( pType == NULL ) ||
        ( pValue == NULL ) ||
        ( pValueLength == NULL ) ||
        ( pCtx->pStart == NULL ) ||
        ( pCtx->currentIndex > pCtx->totalLength ) )
    {
        result = SDP_RESULT_BAD_PARAM;
    }

    if( result == SDP_RESULT_OK )
    {
        if( pCtx->currentIndex == pCtx->totalLength )
        {
            result = SDP_RESULT_MESSAGE_END;
        }
    }

    if( result == SDP_RESULT_OK )
    {
        remainingLength = pCtx->totalLength - pCtx->currentIndex;

        if( remainingLength < 3 )
        {
            result = SDP_RESULT_MESSAGE_MALFORMED_NOT_ENOUGH_INFO;
        }
    }

    if( result == SDP_RESULT_OK )
    {
        /* Check for overflow before accessing array */
        if( sdpADD_WILL_OVERFLOW( pCtx->currentIndex, 1 ) )
        {
            result = SDP_RESULT_MESSAGE_MALFORMED;
        }
        else if( pCtx->currentIndex + 1 >= pCtx->totalLength )
        {
            result = SDP_RESULT_MESSAGE_MALFORMED_NOT_ENOUGH_INFO;
        }
        else if( pCtx->pStart[ pCtx->currentIndex + 1 ] != '=' )
        {
            result = SDP_RESULT_MESSAGE_MALFORMED_EQUAL_NOT_FOUND;
        }
    }

    if( result == SDP_RESULT_OK )
    {
        /* Check for overflow before loop start */
        if( sdpADD_WILL_OVERFLOW( pCtx->currentIndex, 2 ) )
        {
            result = SDP_RESULT_MESSAGE_MALFORMED;
        }
        else
        {
            for( i = pCtx->currentIndex + 2; i < pCtx->totalLength; i++ )
            {
                if( pCtx->pStart[ i ] == '\n' )
                {
                    break;
                }
            }

            if( i == pCtx->totalLength )
            {
                /* No '\n' found. */
                result = SDP_RESULT_MESSAGE_MALFORMED_NEWLINE_NOT_FOUND;
            }
        }

        if( result == SDP_RESULT_OK )
        {
            /* Calculate value length based on line ending */
            size_t offset = ( pCtx->pStart[ i - 1 ] == '\r' ) ? 3 : 2;
            
            /* Check for overflow first */
            if( sdpADD_WILL_OVERFLOW( pCtx->currentIndex, offset ) )
            {
                result = SDP_RESULT_MESSAGE_MALFORMED;
            }
            /* Check bounds */
            else if( i < pCtx->currentIndex + offset )
            {
                result = SDP_RESULT_MESSAGE_MALFORMED_NO_VALUE;
            }
            else
            {
                valueLength = i - pCtx->currentIndex - offset;
                
                if( valueLength > 0 )
                {
                    *pType = pCtx->pStart[ pCtx->currentIndex ];
                    *pValue = &( pCtx->pStart[ pCtx->currentIndex + 2 ] );
                    *pValueLength = valueLength;
                    
                    /* Advance index safely */
                    size_t advance = i - pCtx->currentIndex + 1;
                    if( sdpADD_WILL_OVERFLOW( pCtx->currentIndex, advance ) )
                    {
                        result = SDP_RESULT_MESSAGE_MALFORMED;
                    }
                    else
                    {
                        pCtx->currentIndex += advance;
                    }
                }
                else
                {
                    result = SDP_RESULT_MESSAGE_MALFORMED_NO_VALUE;
                }
            }
        }
    }

    return result;
}
/*-----------------------------------------------------------*/

SdpResult_t SdpDeserializer_ParseOriginator( const char * pValue,
                                             size_t valueLength,
                                             SdpOriginator_t * pOriginator )
{
    SdpResult_t result = SDP_RESULT_OK;
    size_t i, start = 0, numSpaces = 0;
    int sscanfRetVal;

    /* Input check. */
    if( ( pValue == NULL ) ||
        ( pOriginator == NULL ) )
    {
        result = SDP_RESULT_BAD_PARAM;
    }

    if( result == SDP_RESULT_OK )
    {
        for( i = 0; i < valueLength; i++ )
        {
            if( pValue[ i ] == ' ' )
            {
                numSpaces += 1;

                if( numSpaces == 1 )
                {
                    pOriginator->pUserName = &( pValue[ start ] );
                    pOriginator->userNameLength = i - start;
                }
                else if( numSpaces == 2 )
                {
                    sscanfRetVal = sscanf( &( pValue[ start ] ),
                                           "%" SDP_PRINT_FMT_UINT64,
                                           &( pOriginator->sessionId ) );

                    if( sscanfRetVal != 1 )
                    {
                        result = SDP_RESULT_MESSAGE_MALFORMED_NO_SESSION_ID;
                        break;
                    }
                }
                else /* numSpaces == 3 */
                {
                    sscanfRetVal = sscanf( &( pValue[ start ] ),
                                           "%" SDP_PRINT_FMT_UINT64,
                                           &( pOriginator->sessionVersion ) );

                    if( sscanfRetVal != 1 )
                    {
                        result = SDP_RESULT_MESSAGE_MALFORMED_NO_SESSION_VERSION;
                        break;
                    }
                    else
                    {
                        /* Check for overflow before incrementing start */
                        if( sdpADD_WILL_OVERFLOW( i, 1 ) )
                        {
                            result = SDP_RESULT_MESSAGE_MALFORMED;
                            break;
                        }
                        
                        /* Skip over space. */
                        start = i + 1;

                        /* break to parse connection info inside of originator. */
                        break;
                    }
                }

                /* Check for overflow before incrementing start */
                if( sdpADD_WILL_OVERFLOW( i, 1 ) )
                {
                    result = SDP_RESULT_MESSAGE_MALFORMED;
                    break;
                }
                start = i + 1;
            }
        }
    }

    if( result == SDP_RESULT_OK )
    {
        if( i < valueLength )
        {
            /* Originator part is parsed, continue to parse connection info. */
            result = SdpDeserializer_ParseConnectionInfo( &( pValue[ start ] ),
                                                          valueLength - start,
                                                          &( pOriginator->connectionInfo ) );
        }
        else
        {
            result = SDP_RESULT_MESSAGE_MALFORMED_NOT_ENOUGH_INFO;
        }
    }

    return result;
}
/*-----------------------------------------------------------*/

SdpResult_t SdpDeserializer_ParseConnectionInfo( const char * pValue,
                                                 size_t valueLength,
                                                 SdpConnectionInfo_t * pConnInfo )
{
    SdpResult_t result = SDP_RESULT_OK;
    size_t i, start = 0, numSpaces = 0;

    /* Input check. */
    if( ( pValue == NULL ) ||
        ( pConnInfo == NULL ) )
    {
        result = SDP_RESULT_BAD_PARAM;
    }

    if( result == SDP_RESULT_OK )
    {
        for( i = 0; i < valueLength; i++ )
        {
            if( pValue[ i ] == ' ' )
            {
                numSpaces += 1;

                if( numSpaces == 1 )
                {
                    if( ( i - start ) == 2 )
                    {
                        if( strncmp( &( pValue[ start ] ), "IN", 2 ) == 0 )
                        {
                            pConnInfo->networkType = SDP_NETWORK_IN;
                        }
                        else
                        {
                            result = SDP_RESULT_MESSAGE_MALFORMED_INVALID_NETWORK_TYPE;
                            break;
                        }
                    }
                    else
                    {
                        result = SDP_RESULT_MESSAGE_MALFORMED_INVALID_NETWORK_TYPE;
                        break;
                    }
                }
                else if( numSpaces == 2 )
                {
                    if( ( i - start ) == 3 )
                    {
                        if( strncmp( &( pValue[ start ] ), "IP4", 3 ) == 0 )
                        {
                            pConnInfo->addressType = SDP_ADDRESS_IPV4;
                        }
                        else if( strncmp( &( pValue[ start ] ), "IP6", 3 ) == 0 )
                        {
                            pConnInfo->addressType = SDP_ADDRESS_IPV6;
                        }
                        else
                        {
                            result = SDP_RESULT_MESSAGE_MALFORMED_INVALID_ADDRESS_TYPE;
                            break;
                        }
                    }
                    else
                    {
                        result = SDP_RESULT_MESSAGE_MALFORMED_INVALID_ADDRESS_TYPE;
                        break;
                    }
                }
                else
                {
                    result = SDP_RESULT_MESSAGE_MALFORMED_REDUNDANT_INFO;
                }

                /* Check for overflow before incrementing start */
                if( sdpADD_WILL_OVERFLOW( i, 1 ) )
                {
                    result = SDP_RESULT_MESSAGE_MALFORMED;
                    break;
                }
                start = i + 1;
            }
        }
    }

    if( result == SDP_RESULT_OK )
    {
        if( numSpaces == 2 )
        {
            pConnInfo->pAddress = &( pValue[ start ] );
            pConnInfo->addressLength = i - start;
        }
        else
        {
            result = SDP_RESULT_MESSAGE_MALFORMED_NOT_ENOUGH_INFO;
        }
    }

    return result;
}
/*-----------------------------------------------------------*/

SdpResult_t SdpDeserializer_ParseBandwidthInfo( const char * pValue,
                                                size_t valueLength,
                                                SdpBandwidthInfo_t * pBandwidthInfo )
{
    SdpResult_t result = SDP_RESULT_OK;
    size_t i, numColon = 0;
    int sscanfRetVal;

    /* Input check. */
    if( ( pValue == NULL ) ||
        ( pBandwidthInfo == NULL ) )
    {
        result = SDP_RESULT_BAD_PARAM;
    }

    if( result == SDP_RESULT_OK )
    {
        for( i = 0; i < valueLength; i++ )
        {
            if( pValue[ i ] == ':' )
            {
                numColon += 1;
                pBandwidthInfo->pBwType = &( pValue[ 0 ] );
                pBandwidthInfo->bwTypeLength = i;

                /* Check for overflow before array access */
                if( sdpADD_WILL_OVERFLOW( i, 1 ) )
                {
                    result = SDP_RESULT_MESSAGE_MALFORMED;
                    break;
                }
                
                sscanfRetVal = sscanf( &( pValue[ i + 1 ] ),
                                       "%" SDP_PRINT_FMT_UINT64,
                                       &( pBandwidthInfo->sdpBandwidthValue ) );
                if( sscanfRetVal != 1 )
                {
                    result = SDP_RESULT_MESSAGE_MALFORMED_INVALID_BANDWIDTH;
                }

                break;
            }
        }

        if( numColon == 0 )
        {
            result = SDP_RESULT_MESSAGE_MALFORMED_NOT_ENOUGH_INFO;
        }
    }

    return result;
}
/*-----------------------------------------------------------*/

SdpResult_t SdpDeserializer_ParseTimeActive( const char * pValue,
                                             size_t valueLength,
                                             SdpTimeDescription_t * pTimeDescription )
{
    SdpResult_t result = SDP_RESULT_OK;
    size_t i, numSpaces = 0;
    int sscanfRetVal;

    /* Input check. */
    if( ( pValue == NULL ) ||
        ( pTimeDescription == NULL ) )
    {
        result = SDP_RESULT_BAD_PARAM;
    }

    if( result == SDP_RESULT_OK )
    {
        for( i = 0; i < valueLength; i++ )
        {
            if( pValue[ i ] == ' ' )
            {
                numSpaces += 1;

                /* Parse start-time. */
                sscanfRetVal = sscanf( &( pValue[ 0 ] ),
                                       "%" SDP_PRINT_FMT_UINT64,
                                       &( pTimeDescription->startTime ) );

                if( sscanfRetVal != 1 )
                {
                    result = SDP_RESULT_MESSAGE_MALFORMED_INVALID_START_TIME;
                    break;
                }

                /* Check for overflow before array access */
                if( sdpADD_WILL_OVERFLOW( i, 1 ) )
                {
                    result = SDP_RESULT_MESSAGE_MALFORMED;
                    break;
                }
                
                /* Parse stop-time. */
                sscanfRetVal = sscanf( &( pValue[ i + 1 ] ),
                                       "%" SDP_PRINT_FMT_UINT64,
                                       &( pTimeDescription->stopTime ) );

                if( sscanfRetVal != 1 )
                {
                    result = SDP_RESULT_MESSAGE_MALFORMED_INVALID_STOP_TIME;
                }

                break;
            }
        }

        if( numSpaces == 0 )
        {
            result = SDP_RESULT_MESSAGE_MALFORMED_NOT_ENOUGH_INFO;
        }
    }

    return result;
}
/*-----------------------------------------------------------*/

SdpResult_t SdpDeserializer_ParseAttribute( const char * pValue,
                                            size_t valueLength,
                                            SdpAttribute_t * pAttribute )
{
    SdpResult_t result = SDP_RESULT_OK;
    size_t i;

    /* Input check. */
    if( ( pValue == NULL ) ||
        ( pAttribute == NULL ) )
    {
        result = SDP_RESULT_BAD_PARAM;
    }

    if( result == SDP_RESULT_OK )
    {
        for( i = 0; i < valueLength; i++ )
        {
            if( pValue[ i ] == ':' )
            {
                pAttribute->pAttributeName = &( pValue[ 0 ] );
                pAttribute->attributeNameLength = i;

                /* Check for overflow before array access and subtraction */
                if( sdpADD_WILL_OVERFLOW( i, 1 ) || ( i + 1 > valueLength ) )
                {
                    result = SDP_RESULT_MESSAGE_MALFORMED;
                    break;
                }
                
                pAttribute->pAttributeValue = &( pValue[ i + 1 ] );
                pAttribute->attributeValueLength = valueLength - ( i + 1 );
                break;
            }
        }

        /* No ':' in this attribute. */
        if( i == valueLength )
        {
            pAttribute->pAttributeName = &( pValue[ 0 ] );
            pAttribute->attributeNameLength = valueLength;

            pAttribute->pAttributeValue = NULL;
            pAttribute->attributeValueLength = 0;
        }
    }

    return result;
}
/*-----------------------------------------------------------*/

SdpResult_t SdpDeserializer_ParseMedia( const char * pValue,
                                        size_t valueLength,
                                        SdpMedia_t * pMedia )
{
    SdpResult_t result = SDP_RESULT_OK;
    size_t i, j, start = 0, numSpaces = 0;
    int sscanfRetVal;

    /* Input check. */
    if( ( pValue == NULL ) ||
        ( pMedia == NULL ) )
    {
        result = SDP_RESULT_BAD_PARAM;
    }

    if( result == SDP_RESULT_OK )
    {
        for( i = 0; i < valueLength; i++ )
        {
            if( pValue[ i ] == ' ' )
            {
                numSpaces += 1;

                if( numSpaces == 1 )
                {
                    pMedia->pMedia = &( pValue[ start ] );
                    pMedia->mediaLength = i - start;
                }
                else if( numSpaces == 2 )
                {
                    sscanfRetVal = sscanf( &( pValue[ start ] ),
                                           "%" SDP_PRINT_FMT_UINT16,
                                           &( pMedia->port ) );

                    if( sscanfRetVal != 1 )
                    {
                        result = SDP_RESULT_MESSAGE_MALFORMED_INVALID_PORT;
                        break;
                    }

                    pMedia->portNum = 0;

                    for( j = start ; j < i ; j++ )
                    {
                        if( pValue[ j ] == '/' )
                        {
                            /* Check for overflow before array access */
                            if( sdpADD_WILL_OVERFLOW( j, 1 ) )
                            {
                                result = SDP_RESULT_MESSAGE_MALFORMED;
                                break;
                            }
                            
                            sscanfRetVal = sscanf( &( pValue[ j + 1 ] ),
                                                   "%" SDP_PRINT_FMT_UINT16,
                                                   &( pMedia->portNum ) );

                            if( sscanfRetVal != 1 )
                            {
                                result = SDP_RESULT_MESSAGE_MALFORMED_INVALID_PORTNUM;
                            }

                            break;
                        }
                    }
                }
                else /* numSpaces == 3 */
                {
                    pMedia->pProtocol = &( pValue[ start ] );
                    pMedia->protocolLength = i - start;

                    /* Check for overflow before incrementing start */
                    if( sdpADD_WILL_OVERFLOW( i, 1 ) )
                    {
                        result = SDP_RESULT_MESSAGE_MALFORMED;
                        break;
                    }
                    
                    /* Skip last ' ' in protocol. */
                    start = i + 1;
                    break;
                }

                /* Check for overflow before incrementing start */
                if( sdpADD_WILL_OVERFLOW( i, 1 ) )
                {
                    result = SDP_RESULT_MESSAGE_MALFORMED;
                    break;
                }
                start = i + 1;
            }
        }
    }

    if( result == SDP_RESULT_OK )
    {
        if( i < valueLength )
        {
            pMedia->pFmt = &( pValue[ start ] );
            pMedia->fmtLength = valueLength - start;
        }
        else
        {
            result = SDP_RESULT_MESSAGE_MALFORMED_NOT_ENOUGH_INFO;
        }
    }

    return result;
}
/*-----------------------------------------------------------*/
