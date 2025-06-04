## MICROSOFT BIDDING SERVICE

### Bidding Service Multi Bid

Introduction: This class, `MicrosoftGenerateMultiTypeBidsReactor` has the necessary changes to
handle requests that can use single or multiple bids in single requests.

Usage: This class should be used in place of the original `GenerateBidsReactor` class for request
generation and response handling There are no changes to the way this class will be used compared to
the original, the change needed comes in the interest group that needs to specify whether it can be
merged with others into a single request or not.

Features: Generate `DispatchRequests` with multiple interest groups and handle their response in the
`generateBid` is invoked by the framework with one of the following arguments:

1. A single interest group and it's associated data `auction_signals`: passed to all buyers
   `buyer_signals`: any that the DSP "passed to itself" via it's real-time bidding (RTB) service
   call `trusted_bidding_signals`: signals `device_signals`: a.k.a `browserSignals`
2. An Array of interest groups and their associated data `auction_signals` & `buyer_signals`: same
   as for single interest group shared between all interest groups sent in the array
   `trusted_bidding_signals` & `device_signals`: a map of signals. The map content keys are interest
   group names

In the Ad Selection API, multiple bids can be returned for evaluation by the seller's auction code.
Each bid has the same shape as the bid returned in the Protected Audience auction. The `render`
element can take two forms:

1. A single string representing the `renderURL`.
2. An object with renderURL and width and height, which the frame will use to help size the frame
   that the ad appears in. Note that in the case of using the size option, k-anon will include the
   size in its tuple. The response returned is a json string array containing a single or multiple
   Interest Groups

#### Return example structure:

```javascript
generateBids(interest_groups, auction_signals, buyer_signals, trusted_bidding_signals, device_signals) {
  ...
  return {
        response: [{ 'ad': adObject,
                    'bid': bidValue,
                    'render': renderUrl,
                    'adComponents': ["adComponentRenderUrlOne", "adComponentRenderUrlTwo"],
                    'allowComponentAuction': false,
                    'interestGroupName': ig_name,
                    ...
                  }],
        logs: ps_logs,
        errors: ps_errors,
        warnings: ps_warns
      }
}
```

#### Response examples:

```json
// A single response json string
{
    "response": [
        {
            "render": "https://adTech.com/ad?id=123",
            "bid": 1,
            "interestGroupName": "ig_name_Foo"
        }
    ],
    "logs": ["test log"],
    "errors": ["test.error"],
    "warnings": ["test.warn"]
}
```

```json
// A json string response array for multiple Interest Groups
{
    "response": [
        { "render": "https://adTech.com/ad?id=123", "bid": 1, "interestGroupName": "ig_name_Foo" },
        { "render": "https://adTech.com/ad?id=456", "bid": 2, "interestGroupName": "ig_name_Bar" }
    ],
    "logs": ["test log"],
    "errors": ["test.error"],
    "warnings": ["test.warn"]
}
```
